#include "pch.h"
#include "HoldemPokerGame.h"
#include "HoldemTableSnapshot.h"
#include "GameItem/HandEvaluator.h"
#include "Net/NetPack.h"
#include "Const.h"
#include <algorithm>
#include <unordered_map>

HoldemPokerGame::HoldemPokerGame()
	: _rng(std::random_device{}())
{
}

int HoldemPokerGame::s_maxSeats = 0;

void HoldemPokerGame::SetMaxSeats(int maxSeats)
{
	s_maxSeats = maxSeats;
}

int HoldemPokerGame::GetMaxSeats()
{
	return s_maxSeats;
}

HoldemPokerGame::SetBlindsResult HoldemPokerGame::SetBlinds(int smallBlind, int bigBlind)
{
	if (_stage != Stage::Waiting)
		return SetBlindsResult::GameInProgress;

	for (const Seat& seat : _seats)
	{
		if (seat.IsOccupied())
			return SetBlindsResult::PlayersSeated;
	}

	if (smallBlind <= 0 || bigBlind <= 0)
		return SetBlindsResult::InvalidValue;

	_smallBlind = smallBlind;
	_bigBlind = bigBlind;
	_minBuyin = _bigBlind * 100;

	return SetBlindsResult::Success;
}

bool HoldemPokerGame::CanStart() const
{
	if (!AreBlindsSet())
		return false;

	return _stage == Stage::Waiting && ActiveSeatCount() >= 2;
}

bool HoldemPokerGame::IsSeatActive(const Seat& seat) const
{
	return seat.IsOccupied() && !seat.sittingOut && seat.chips >= _bigBlind;
}

void HoldemPokerGame::StartHand()
{
	if (!CanStart()) return;

	for (Seat& seat : _seats)
	{
		if (!seat.IsOccupied()) continue;
		if (seat.chips < _bigBlind && !seat.sittingOut)
			seat.sittingOut = true;
	}
	if (ActiveSeatCount() < 2) return;

	_stage = Stage::PreFlop;
	_community.clear();
	_sidePots.clear();
	_deck.Reset52();
	_deck.Shuffle(_rng);

	for (Seat& seat : _seats)
	{
		seat.currentBet = 0;
		seat.totalBetThisHand = 0;
		seat.inHand = false;
		seat.folded = false;
		seat.allIn = false;
		if (IsSeatActive(seat))
			seat.inHand = true;
	}

	if (!_seats.empty())
	{
		size_t newDealerIndex = FindNextValidBlindPosition(_dealerIndex + 1);
		if (newDealerIndex < _seats.size())
			_dealerIndex = newDealerIndex;
	}

	PostBlinds();
	for (Seat& seat : _seats)
		seat.actedThisRound = false;

	DealHoleCards();
	size_t sbPos = FindNextValidBlindPosition(_dealerIndex + 1);
	size_t bbPos = FindNextValidBlindPosition(sbPos + 1);
	if (bbPos < _seats.size())
		_actingIndex = NextActiveIndex(bbPos + 1);
	else
		_actingIndex = _seats.size();
	
#ifdef ENABLE_HOLDEM_POKER_DEBUG
	std::cout << "post blinds debug: " << std::endl;
	std::cout << "_dealerIndex: " << _dealerIndex << std::endl;
	std::cout << "sbPos: " << sbPos << std::endl;
	std::cout << "bbPos: " << bbPos << std::endl;
	std::cout << "_actingIndex: " << _actingIndex << std::endl;
#endif
}

void HoldemPokerGame::PostBlinds()
{
	if (_seats.empty() || _dealerIndex >= _seats.size()) return;

	size_t sbPos = _seats.size();
	size_t bbPos = _seats.size();
	sbPos = FindNextValidBlindPosition(_dealerIndex + 1);
	bbPos = FindNextValidBlindPosition(sbPos + 1);

	if (sbPos >= _seats.size() || bbPos >= _seats.size()) return;

	Seat& sbSeat = _seats[sbPos];
	int sbAmount = std::min(_smallBlind, sbSeat.chips);
	sbSeat.chips -= sbAmount;
	sbSeat.currentBet = sbAmount;
	sbSeat.totalBetThisHand = sbAmount;
	if (sbSeat.chips == 0) sbSeat.allIn = true;
	RecordLastAction(sbSeat.playerId, Action::Bet, sbAmount);

	Seat& bbSeat = _seats[bbPos];
	int bbAmount = std::min(_bigBlind, bbSeat.chips);
	bbSeat.chips -= bbAmount;
	bbSeat.currentBet = bbAmount;
	bbSeat.totalBetThisHand = bbAmount;
	if (bbSeat.chips == 0) bbSeat.allIn = true;
	RecordLastAction(bbSeat.playerId, Action::Bet, bbAmount);

	_lastBet = bbAmount;
	_lastRaise = _bigBlind;
}

size_t HoldemPokerGame::FindNextValidBlindPosition(size_t start) const
{
	if (_seats.empty()) return _seats.size();
	for (size_t offset = 0; offset < _seats.size(); ++offset)
	{
		size_t idx = (start + offset) % _seats.size();
		const Seat& seat = _seats[idx];
		if (seat.inHand)
			return idx;
	}
	return _seats.size();
}

void HoldemPokerGame::AdvanceStage()
{
	CollectBetsToSidePots();

	switch (_stage)
	{
	case Stage::PreFlop:
		_stage = Stage::Flop;
		DealCommunity(3);
		break;
	case Stage::Flop:
		_stage = Stage::Turn;
		DealCommunity(1);
		break;
	case Stage::Turn:
		_stage = Stage::River;
		DealCommunity(1);
		break;
	case Stage::River:
		_stage = Stage::Showdown;
		HandleShowdown();
		FinishHand();
		return;
	default:
		return;
	}
	ResetBetsForNewRound();
	_actingIndex = NextActiveIndex(_dealerIndex + 1);
}

void HoldemPokerGame::CollectBetsToSidePots()
{
	std::vector<std::pair<int, size_t>> bets; // (betAmount, seatIndex)
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		const Seat& seat = _seats[i];
		if (!seat.IsOccupied() || !seat.inHand) continue;
		if (seat.currentBet > 0)
			bets.emplace_back(seat.currentBet, i);
	}

	if (bets.empty()) return;

	std::sort(bets.begin(), bets.end());

	int previousLevel = 0;
	for (size_t i = 0; i < bets.size(); ++i)
	{
		int currentLevel = bets[i].first;
		if (currentLevel <= previousLevel) continue;

		int contribution = currentLevel - previousLevel;
		SidePot pot;

		for (size_t j = 0; j < _seats.size(); ++j)
		{
			Seat& seat = _seats[j];
			if (!seat.IsOccupied() || !seat.inHand) continue;
			if (seat.currentBet >= currentLevel)
			{
				pot.amount += contribution;
				if (!seat.folded)
					pot.eligiblePlayerIds.push_back(seat.playerId);
			}
			else if (seat.currentBet > previousLevel)
			{
				pot.amount += seat.currentBet - previousLevel;
				if (!seat.folded)
					pot.eligiblePlayerIds.push_back(seat.playerId);
			}
		}

		if (pot.amount > 0 && !pot.eligiblePlayerIds.empty())
			_sidePots.push_back(pot);

		previousLevel = currentLevel;
	}

	for (Seat& seat : _seats)
		seat.currentBet = 0;
}

void HoldemPokerGame::ResetBetsForNewRound()
{
	_lastBet = 0;
	_lastRaise = _bigBlind;
	for (Seat& seat : _seats)
	{
		seat.currentBet = 0;
		seat.actedThisRound = false;
	}
}

void HoldemPokerGame::AdvanceTurn()
{
	if (_stage == Stage::Waiting || _seats.empty())
		return;

	if (AllBetsMatched() && AllActivePlayersActed())
	{
		AdvanceStage();
		return;
	}

	size_t next = NextActiveIndex(_actingIndex + 1);
	if (next >= _seats.size())
	{
		AdvanceStage();
		return;
	}
	_actingIndex = next;
}

HoldemPokerGame::ActionResult HoldemPokerGame::HandleAction(int playerId, Action action, int amount)
{
	if (_stage == Stage::Waiting)
	{
		if (CanStart())
			StartHand();
		return ActionResult::Ignored;
	}

	Seat* seat = GetSeatByPlayerId(playerId);
	if (!seat) return ActionResult::Invalid;

	if (_seats.empty() || _actingIndex >= _seats.size())
		return ActionResult::Invalid;
	if (_seats[_actingIndex].playerId != playerId)
		return ActionResult::Invalid;
	if (!seat->CanAct())
		return ActionResult::Invalid;

	int toCall = std::max(0, _lastBet - seat->currentBet);
	bool didRaise = false;
	int paid = 0;
	const int maxAllowedTotal = GetMaxAllowedTotalBet(*seat);

	switch (action)
	{
	case Action::CheckCall:
	{
		int pay = std::min(seat->chips, toCall);
		seat->chips -= pay;
		seat->currentBet += pay;
		seat->totalBetThisHand += pay;
		if (seat->chips == 0)
			seat->allIn = true;
		paid = pay;
		break;
	}
	case Action::Bet:
	{
		if (_lastBet > 0)
			return ActionResult::Invalid;

		if (amount > maxAllowedTotal)
			amount = maxAllowedTotal;

		int maxTotal = seat->currentBet + seat->chips;
		if (amount <= seat->currentBet || amount > maxTotal)
			return ActionResult::Invalid;

		bool isAllIn = amount == maxTotal;
		if (!isAllIn && amount < _bigBlind)
			return ActionResult::Invalid;

		int pay = amount - seat->currentBet;
		pay = std::min(pay, seat->chips);
		seat->chips -= pay;
		seat->currentBet = amount;
		seat->totalBetThisHand += pay;
		_lastBet = seat->currentBet;
		_lastRaise = std::max(_bigBlind, _lastBet);
		if (seat->chips == 0)
			seat->allIn = true;
		paid = pay;
		didRaise = true;
		break;
	}
	case Action::Raise:
	{
		if (_lastBet <= 0)
			return ActionResult::Invalid;
		if (amount < 0)
			return ActionResult::Invalid;

		int maxTotal = seat->currentBet + seat->chips;
		int desiredTotal = _lastBet + amount;
		if (desiredTotal > maxAllowedTotal)
			desiredTotal = maxAllowedTotal;
		bool isAllIn = desiredTotal >= maxTotal;
		bool isCapped = desiredTotal >= maxAllowedTotal;

		if (desiredTotal <= seat->currentBet)
			return ActionResult::Invalid;

		int raiseAmount = desiredTotal - _lastBet;
		if (!isAllIn && !isCapped)
		{
			if (raiseAmount <= 0 || raiseAmount < _lastRaise)
				return ActionResult::Invalid;
		}

		int pay = desiredTotal - seat->currentBet;
		pay = std::min(pay, seat->chips);
		seat->chips -= pay;
		seat->currentBet = desiredTotal;
		seat->totalBetThisHand += pay;
		paid = pay;

		int previousLastBet = _lastBet;
		if (desiredTotal > _lastBet)
			_lastBet = desiredTotal;

		if (seat->chips == 0)
			seat->allIn = true;

		int actualRaise = desiredTotal - previousLastBet;
		if (actualRaise >= _lastRaise)
		{
			_lastRaise = actualRaise;
			didRaise = true;
		}
		break;
	}
	case Action::AllIn:
	{
		int maxTotal = seat->currentBet + seat->chips;
		if (maxTotal <= seat->currentBet)
			return ActionResult::Invalid;

		if (_lastBet == 0)
		{
			int desiredTotal = std::min(maxTotal, maxAllowedTotal);
			int pay = desiredTotal - seat->currentBet;
			seat->chips -= pay;
			seat->currentBet = desiredTotal;
			seat->totalBetThisHand += pay;
			_lastBet = seat->currentBet;
			_lastRaise = std::max(_bigBlind, _lastBet);
			seat->allIn = (seat->chips == 0);
			paid = pay;
			didRaise = true;
			break;
		}

		int previousLastBet = _lastBet;
		int desiredTotal = std::min(maxTotal, maxAllowedTotal);
		int pay = desiredTotal - seat->currentBet;
		pay = std::min(pay, seat->chips);
		seat->chips -= pay;
		seat->currentBet = desiredTotal;
		seat->totalBetThisHand += pay;
		paid = pay;
		if (desiredTotal > _lastBet)
			_lastBet = desiredTotal;
		seat->allIn = (seat->chips == 0);

		int actualRaise = desiredTotal - previousLastBet;
		if (actualRaise >= _lastRaise)
		{
			_lastRaise = actualRaise;
			didRaise = true;
		}
		break;
	}
	case Action::Fold:
	{
		seat->folded = true;
		paid = 0;
		break;
	}
	default:
		return ActionResult::Invalid;
	}

	seat->actedThisRound = true;
	RecordLastAction(playerId, action, paid);
	if (didRaise)
	{
		for (Seat& other : _seats)
		{
			if (&other == seat)
				continue;
			if (!other.IsOccupied() || !other.inHand || other.folded || other.allIn)
				continue;
			other.actedThisRound = false;
		}
	}

	ResolveIfNeeded();
	if (_stage != Stage::Waiting)
		AdvanceTurn();

	return ActionResult::Success;
}

void HoldemPokerGame::ProcessAutoModePlayer()
{
	if (_stage == Stage::Waiting || _seats.empty())
		return;
	if (_actingIndex >= _seats.size())
		_actingIndex = 0;

	Seat& seat = _seats[_actingIndex];
	if (!seat.CanAct())
		return;

	if (seat.autoMode || seat.pendingLeave)
	{
		int toCall = std::max(0, _lastBet - seat.currentBet);
		if (toCall == 0)
		{
			RecordLastAction(seat.playerId, Action::CheckCall, 0);
		}
		else
		{
			seat.folded = true;
			RecordLastAction(seat.playerId, Action::Fold, 0);
		}
		seat.actedThisRound = true;
		ResolveIfNeeded();
		if (_stage != Stage::Waiting)
			AdvanceTurn();
	}
}

void HoldemPokerGame::ResolveIfNeeded()
{
	size_t aliveIdx = 0;
	int aliveCnt = 0;
	bool allIn = true;
	int withChips = 0;
	const Seat* loneWithChips = nullptr;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		const Seat& seat = _seats[i];
		if (!seat.IsOccupied() || !seat.inHand || seat.folded)
			continue;
		aliveCnt++;
		aliveIdx = i;
		if (!seat.allIn)
			allIn = false;
		if (seat.chips > 0)
		{
			withChips++;
			loneWithChips = &seat;
		}
	}

	if (aliveCnt == 0)
	{
		FinishHand();
		return;
	}

	if (aliveCnt == 1)
	{
		CollectBetsToSidePots();
		int totalWin = GetTotalPot();
		_seats[aliveIdx].chips += totalWin;
		
		// Record result for single winner (others folded)
		RecordHandResult(totalWin, false);
		_lastHandResult.playerResults.clear();
		for (const Seat& s : _seats)
		{
			if (!s.inHand) continue;
			PlayerHandResult pr;
			pr.playerId = s.playerId;
			pr.folded = s.folded;
			pr.holeCards[0] = Card{};
			pr.holeCards[1] = Card{};
			pr.handRank = s.folded ? 0 : EvaluateHand(s);
			pr.chipsWon = (s.seatIndex == static_cast<int>(aliveIdx)) ? totalWin : 0;
			_lastHandResult.playerResults.push_back(pr);
		}
		_hasPendingHandResult = true;
		
		_sidePots.clear();
		FinishHand();
		return;
	}

	if (aliveCnt > 1 && (allIn || (withChips == 1 && loneWithChips
		&& loneWithChips->currentBet >= _lastBet)))
	{
		while (_community.size() < 5)
			DealCommunity(1);
		_stage = Stage::Showdown;
		HandleShowdown();
		FinishHand();
		return;
	}

	if (_stage == Stage::Showdown)
	{
		HandleShowdown();
		FinishHand();
	}
}

void HoldemPokerGame::FinishHand()
{
	_stage = Stage::Waiting;
	_community.clear();
	_sidePots.clear();
	_lastBet = 0;
	_lastRaise = 0;

	for (Seat& seat : _seats)
	{
		seat.inHand = false;
		seat.folded = false;
		seat.allIn = false;
		seat.currentBet = 0;
		seat.totalBetThisHand = 0;
		seat.actedThisRound = false;
	}
}

bool HoldemPokerGame::SitDown(int playerId, int seatIdxHint, int& actualSeatIdx)
{
	(void)seatIdxHint;
	if (GetSeatByPlayerId(playerId) != nullptr)
	{
		actualSeatIdx = -1;
		return false;
	}

	if (!AreBlindsSet())
	{
		actualSeatIdx = -1;
		return false;
	}

	if (s_maxSeats <= 0)
	{
		actualSeatIdx = -1;
		return false;
	}

	int seatIdx = -1;
	for (int i = 0; i < s_maxSeats; ++i)
	{
		if (GetSeatByIndex(i) == nullptr)
		{
			seatIdx = i;
			break;
		}
	}

	if (seatIdx < 0)
	{
		actualSeatIdx = -1;
		return false;
	}

	Seat seat{};
	seat.seatIndex = seatIdx;
	seat.playerId = playerId;
	seat.chips = 0;
	seat.sittingOut = true;
	_seats.push_back(seat);

	std::sort(_seats.begin(), _seats.end(), [](const Seat& a, const Seat& b) {
		return a.seatIndex < b.seatIndex;
	});

	actualSeatIdx = seatIdx;
	return true;
}

HoldemPokerGame::BuyInResult HoldemPokerGame::BuyIn(int playerId, int amount)
{
	Seat* seat = GetSeatByPlayerId(playerId);
	if (!seat)
		return BuyInResult::PlayerNotFound;

	if (amount < _minBuyin)
		return BuyInResult::BelowMinimum;

	if (seat->inHand)
		return BuyInResult::AlreadyInHand;

	seat->chips += amount;
	if (seat->chips >= _bigBlind && seat->sittingOut)
		seat->sittingOut = false;

	return BuyInResult::Success;
}

bool HoldemPokerGame::StandUp(int playerId)
{
	Seat* seat = GetSeatByPlayerId(playerId);
	if (!seat) return false;

	seat->sittingOut = true;
	if (seat->inHand)
		seat->autoMode = true;
	return true;
}

bool HoldemPokerGame::SitBack(int playerId)
{
	Seat* seat = GetSeatByPlayerId(playerId);
	if (!seat) return false;

	if (seat->chips < _bigBlind)
		return false;

	seat->sittingOut = false;
	seat->autoMode = false;
	return true;
}

void HoldemPokerGame::MarkPendingLeave(int playerId)
{
	Seat* seat = GetSeatByPlayerId(playerId);
	if (seat)
	{
		seat->pendingLeave = true;
		seat->autoMode = true;
	}
}

void HoldemPokerGame::RemovePendingLeavers()
{
	int dealerSeatIndex = -1;
	int actingPlayerId = -1;
	if (!_seats.empty() && _dealerIndex < _seats.size())
		dealerSeatIndex = _seats[_dealerIndex].seatIndex;
	if (!_seats.empty() && _actingIndex < _seats.size())
		actingPlayerId = _seats[_actingIndex].playerId;

	_seats.erase(std::remove_if(_seats.begin(), _seats.end(), [](const Seat& s) {
		return s.playerId < 0 || (s.pendingLeave && !s.inHand);
	}), _seats.end());

	if (_seats.empty())
	{
		_dealerIndex = 0;
		_actingIndex = 0;
	}
	else
	{
		if (actingPlayerId >= 0)
		{
			auto actingIt = std::find_if(_seats.begin(), _seats.end(), [actingPlayerId](const Seat& seat) {
				return seat.playerId == actingPlayerId;
			});
			_actingIndex = (actingIt != _seats.end())
				? static_cast<size_t>(std::distance(_seats.begin(), actingIt))
				: 0;
		}
		else
		{
			_actingIndex = 0;
		}

		if (dealerSeatIndex >= 0)
		{
			auto dealerIt = std::find_if(_seats.begin(), _seats.end(), [dealerSeatIndex](const Seat& seat) {
				return seat.seatIndex == dealerSeatIndex;
			});
			if (dealerIt != _seats.end())
			{
				_dealerIndex = static_cast<size_t>(std::distance(_seats.begin(), dealerIt));
			}
			else
			{
				size_t prevIdx = _seats.size();
				for (size_t i = 0; i < _seats.size(); ++i)
				{
					if (_seats[i].seatIndex < dealerSeatIndex)
						prevIdx = i;
				}
				if (prevIdx == _seats.size())
					prevIdx = _seats.size() - 1;
				_dealerIndex = prevIdx;
			}
		}
		else
		{
			_dealerIndex = 0;
		}
	}
}

bool HoldemPokerGame::HasAvailableSeat() const
{
	if (s_maxSeats <= 0)
		return false;

	for (int i = 0; i < s_maxSeats; ++i)
	{
		if (GetSeatByIndex(i) == nullptr)
			return true;
	}
	return false;
}

int HoldemPokerGame::GetPlayerChips(int playerId) const
{
	const Seat* seat = GetSeatByPlayerId(playerId);
	return seat ? seat->chips : 0;
}

int HoldemPokerGame::GetDealerSeatIndex() const
{
	if (_seats.empty() || _dealerIndex >= _seats.size())
		return -1;
	return _seats[_dealerIndex].seatIndex;
}

int HoldemPokerGame::GetSmallBlindSeatIndex() const
{
	if (_seats.empty() || _dealerIndex >= _seats.size())
		return -1;
	size_t sbPos = FindNextValidBlindPosition(_dealerIndex + 1);
	return (sbPos < _seats.size()) ? _seats[sbPos].seatIndex : -1;
}

int HoldemPokerGame::GetBigBlindSeatIndex() const
{
	if (_seats.empty() || _dealerIndex >= _seats.size())
		return -1;
	size_t sbPos = FindNextValidBlindPosition(_dealerIndex + 1);
	if (sbPos >= _seats.size())
		return -1;
	size_t bbPos = FindNextValidBlindPosition(sbPos + 1);
	return (bbPos < _seats.size()) ? _seats[bbPos].seatIndex : -1;
}

int HoldemPokerGame::CashOut(int playerId)
{
	Seat* seat = GetSeatByPlayerId(playerId);
	if (!seat)
		return 0;

	if (seat->inHand)
		return 0;

	int chips = seat->chips;
	seat->chips = 0;
	seat->sittingOut = true;
	return chips;
}

const Seat* HoldemPokerGame::GetSeatByPlayerId(int playerId) const
{
	for (const Seat& seat : _seats)
		if (seat.playerId == playerId)
			return &seat;
	return nullptr;
}

Seat* HoldemPokerGame::GetSeatByPlayerId(int playerId)
{
	return const_cast<Seat*>(
		static_cast<const HoldemPokerGame*>(this)->GetSeatByPlayerId(playerId));
}

const Seat* HoldemPokerGame::GetSeatByIndex(int seatIdx) const
{
	for (const Seat& seat : _seats)
		if (seat.seatIndex == seatIdx)
			return &seat;
	return nullptr;
}

int HoldemPokerGame::ActiveSeatCount() const
{
	int cnt = 0;
	for (const Seat& seat : _seats)
		if (IsSeatActive(seat))
			cnt++;
	return cnt;
}

int HoldemPokerGame::ActingPlayerId() const
{
	if (_stage == Stage::Waiting || _seats.empty() || _actingIndex >= _seats.size())
		return -1;
	const Seat& seat = _seats[_actingIndex];
	if (!seat.folded && seat.inHand)
		return seat.playerId;
	return -1;
}

int HoldemPokerGame::GetTotalPot() const
{
	int total = 0;
	for (const SidePot& pot : _sidePots)
		total += pot.amount;
	for (const Seat& seat : _seats)
		total += seat.currentBet;
	return total;
}

void HoldemPokerGame::WriteTable(NetPack& pack, int viewerPlayerId) const
{
	HoldemTableSnapshot snapshot = HoldemTableSnapshot::Build(*this, viewerPlayerId);
	snapshot.Write(pack);
}

void HoldemPokerGame::ReadTable(NetPack& pack)
{
	HoldemTableSnapshot snapshot{};
	snapshot.Read(pack);

	_stage = snapshot.stage;
	_lastBet = snapshot.lastBet;
	_dealerIndex = snapshot.dealerSeatIndex;
	_smallBlind = snapshot.smallBlind;
	_bigBlind = snapshot.bigBlind;
	_lastRaise = snapshot.lastRaise;
	_sidePots = snapshot.sidePots;
	_community = snapshot.community;
	_lastActionPlayerId = snapshot.lastActionPlayerId;
	_lastAction = snapshot.lastAction;
	_lastActionAmount = snapshot.lastActionAmount;

	_seats.clear();
	_seats.reserve(snapshot.seats.size());
	_actingIndex = 0;
	for (const auto& seatSnapshot : snapshot.seats)
	{
		_seats.push_back(seatSnapshot.seat);
		if (_seats.back().playerId == snapshot.actingPlayerId)
			_actingIndex = _seats.size() - 1;
	}
}

void HoldemPokerGame::DealHoleCards()
{
	for (Seat& seat : _seats)
	{
		if (!seat.inHand) continue;
		seat.hole[0] = _deck.Draw();
		seat.hole[1] = _deck.Draw();
	}
}

void HoldemPokerGame::DealCommunity(size_t count)
{
	for (size_t i = 0; i < count; ++i)
		_community.push_back(_deck.Draw());
}

size_t HoldemPokerGame::NextActiveIndex(size_t start, bool includeAllIn) const
{
	if (_seats.empty()) return _seats.size();
	for (size_t offset = 0; offset < _seats.size(); ++offset)
	{
		size_t idx = (start + offset) % _seats.size();
		const Seat& seat = _seats[idx];
		if (!seat.IsOccupied() || !seat.inHand || seat.folded)
			continue;
		if (!includeAllIn && seat.allIn)
			continue;
		return idx;
	}
	return _seats.size();
}

bool HoldemPokerGame::AllBetsMatched() const
{
	int activeCount = 0;
	for (const Seat& seat : _seats)
	{
		if (!seat.IsOccupied() || !seat.inHand || seat.folded)
			continue;
		if (seat.allIn)
			continue;
		activeCount++;
		if (!seat.actedThisRound && seat.currentBet != _lastBet)
			return false;
	}
	return activeCount > 0;
}

bool HoldemPokerGame::AllActivePlayersActed() const
{
	int activeCount = 0;
	for (const Seat& seat : _seats)
	{
		if (!seat.IsOccupied() || !seat.inHand || seat.folded)
			continue;
		if (seat.allIn)
			continue;
		activeCount++;
		if (!seat.actedThisRound)
			return false;
	}
	return activeCount > 0;
}

void HoldemPokerGame::HandleShowdown()
{
	CollectBetsToSidePots();
	DistributePots();
}

void HoldemPokerGame::DistributePots()
{
	std::unordered_map<int, int> chipsWonByPlayer{};
	int totalPot = 0;
	for (SidePot& pot : _sidePots)
	{
		totalPot += pot.amount;
		if (pot.amount <= 0 || pot.eligiblePlayerIds.empty())
			continue;

		int bestScore = -1;
		std::vector<int> winners;

		for (int pid : pot.eligiblePlayerIds)
		{
			const Seat* seat = GetSeatByPlayerId(pid);
			if (!seat || seat->folded) continue;

			int score = EvaluateHand(*seat);
			if (score > bestScore)
			{
				bestScore = score;
				winners.clear();
				winners.push_back(pid);
			}
			else if (score == bestScore)
			{
				winners.push_back(pid);
			}
		}

		if (winners.empty()) continue;

		int gain = pot.amount / static_cast<int>(winners.size());
		int remainder = pot.amount % static_cast<int>(winners.size());
		for (int pid : winners)
		{
			Seat* seat = GetSeatByPlayerId(pid);
			if (seat)
				seat->chips += gain;
			chipsWonByPlayer[pid] += gain;
		}
		if (remainder > 0)
		{
			std::vector<int> ordered = winners;
			if (!_seats.empty() && _dealerIndex < _seats.size())
			{
				size_t start = (_dealerIndex + 1) % _seats.size();
				auto seatPos = [this](int pid) -> size_t {
					for (size_t i = 0; i < _seats.size(); ++i)
					{
						if (_seats[i].playerId == pid)
							return i;
					}
					return _seats.size();
				};
				std::sort(ordered.begin(), ordered.end(), [this, start, &seatPos](int a, int b) {
					size_t posA = seatPos(a);
					size_t posB = seatPos(b);
					size_t distA = (posA + _seats.size() - start) % _seats.size();
					size_t distB = (posB + _seats.size() - start) % _seats.size();
					if (distA != distB)
						return distA < distB;
					return a < b;
				});
			}
			for (int i = 0; i < remainder && i < static_cast<int>(ordered.size()); ++i)
			{
				int pid = ordered[i];
				Seat* seat = GetSeatByPlayerId(pid);
				if (seat)
					seat->chips += 1;
				chipsWonByPlayer[pid] += 1;
			}
		}
		pot.amount = 0;
	}
	_sidePots.clear();

	RecordHandResult(totalPot, true);
	_lastHandResult.playerResults.clear();
	for (const Seat& s : _seats)
	{
		if (!s.inHand) continue;
		PlayerHandResult pr;
		pr.playerId = s.playerId;
		pr.folded = s.folded;
		pr.holeCards[0] = s.hole[0];
		pr.holeCards[1] = s.hole[1];
		pr.handRank = s.folded ? 0 : EvaluateHand(s);
		auto it = chipsWonByPlayer.find(s.playerId);
		pr.chipsWon = (it != chipsWonByPlayer.end()) ? it->second : 0;
		_lastHandResult.playerResults.push_back(pr);
	}
	_hasPendingHandResult = true;
}

int HoldemPokerGame::EvaluateHand(const Seat& seat) const
{
	std::array<Card, 7> cards{};
	cards[0] = seat.hole[0];
	cards[1] = seat.hole[1];
	for (size_t i = 0; i < _community.size() && i < 5; ++i)
		cards[2 + i] = _community[i];
	return HandEvaluator::Evaluate(cards);
}

void HoldemPokerGame::RecordHandResult(int totalPot, bool isShowdown)
{
	_lastHandResult.Clear();
	_lastHandResult.totalPot = totalPot;
	_lastHandResult.isShowdown = isShowdown;
	_lastHandResult.communityCards = _community;
}

void HoldemPokerGame::RecordLastAction(int playerId, Action action, int amount)
{
	_lastActionPlayerId = playerId;
	_lastAction = action;
	_lastActionAmount = amount;
}

void HoldemPokerGame::ClearLastAction()
{
	_lastActionPlayerId = -1;
	_lastAction = Action::CheckCall;
	_lastActionAmount = 0;
}

int HoldemPokerGame::GetMaxOtherEffectiveTotal(int playerId) const
{
	int maxTotal = 0;
	for (const Seat& seat : _seats)
	{
		if (!seat.IsOccupied() || !seat.inHand || seat.folded)
			continue;
		if (seat.playerId == playerId)
			continue;
		int total = seat.currentBet + seat.chips;
		if (total > maxTotal)
			maxTotal = total;
	}
	return maxTotal;
}

int HoldemPokerGame::GetMaxAllowedTotalBet(const Seat& seat) const
{
	int maxTotal = seat.currentBet + seat.chips;
	int maxOtherTotal = GetMaxOtherEffectiveTotal(seat.playerId);
	if (maxOtherTotal > 0)
		maxTotal = std::min(maxTotal, maxOtherTotal);
	if (maxTotal < seat.currentBet)
		maxTotal = seat.currentBet;
	return maxTotal;
}
