#include "pch.h"
#include "PokerRoom.h"
#include "RoomMgr.h"
#include "Net/NetPack.h"
#include <algorithm>
#include <random>

void PokerRoom::OnPlayerExit(std::shared_ptr<Player> player)
{
	{
		auto wLock = _lock.OnWrite();
		auto seat = FindSeatByPlayer(player);
		if (seat)
		{
			seat->pendingLeave = true;
			if (_stage == Stage::Waiting)
				seat->inHand = false;
		}
	}
	Room::OnPlayerExit(player);
	bool doRemoveRoom = false;
	{
		auto wLock = _lock.OnWrite();
		if (_roomExpired == false && GetPlayerCnt() == 0)
		{
			_roomExpired = true;
			doRemoveRoom = true;
		}
	}
	if (doRemoveRoom)
		RoomMgr::RemoveRoom(_roomId);
}

RpcError PokerRoom::OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack)
{
	switch (pack.MsgType())
	{
	case RpcEnum::rpc_server_get_poker_table_info:
		SendTableInfoTo(player);
		return RpcError::SUCCESS;
	case RpcEnum::rpc_server_sit_down:
	{
		int seatIdx = pack.ReadInt32();
		HandleSitDown(player, seatIdx);
		return RpcError::SUCCESS;
	}
	case RpcEnum::rpc_server_poker_action:
	{
		auto action = pack.ReadUInt8();
		int amount = pack.ReadInt32();
		HandlePlayerAction(player, action, amount);
		return RpcError::SUCCESS;
	}
	default:
		return Room::OnRecvPlayerNetPack(player, pack);
	}
}

void PokerRoom::OnTick()
{
	{
		auto wLock = _lock.OnWrite();
		RemoveEmptySeats();
		if (_stage == Stage::Waiting && ActiveSeatCount() >= 2)
			StartHand();
		AutoFoldIfNeeded();
		ResolveIfNeeded();
	}
	BroadcastTableInfo();
}

void PokerRoom::BuildDeck()
{
	_deck.clear();
	for (int s = 0; s < 4; ++s)
		for (int r = 2; r <= 14; ++r)
			_deck.push_back(Card{ (uint8_t)r, (uint8_t)s });
}

void PokerRoom::ShuffleDeck()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::shuffle(_deck.begin(), _deck.end(), gen);
}

PokerRoom::Card PokerRoom::DrawCard()
{
	if (_deck.empty())
		return Card{};
	Card c = _deck.back();
	_deck.pop_back();
	return c;
}

void PokerRoom::DealHoleCards()
{
	for (auto& seat : _seats)
	{
		if (!seat.player || seat.pendingLeave || seat.chips <= 0)
			continue;
		seat.inHand = true;
		seat.folded = false;
		seat.allIn = false;
		seat.currentBet = 0;
		seat.hole[0] = DrawCard();
		seat.hole[1] = DrawCard();
	}
}

void PokerRoom::DealCommunity(size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i)
		_community.push_back(DrawCard());
}

void PokerRoom::ResetBets()
{
	_lastBet = 0;
	for (auto& seat : _seats)
		seat.currentBet = 0;
}

size_t PokerRoom::NextActiveIndex(size_t start, bool includeAllIn)
{
	if (_seats.empty()) return _seats.size();
	for (size_t offset = 0; offset < _seats.size(); ++offset)
	{
		size_t idx = (start + offset) % _seats.size();
		auto& seat = _seats[idx];
		if (!seat.player || seat.pendingLeave || !seat.inHand || seat.folded)
			continue;
		if (!includeAllIn && seat.allIn)
			continue;
		return idx;
	}
	return _seats.size();
}

PokerRoom::Seat* PokerRoom::FindSeatByPlayer(const std::shared_ptr<Player>& player)
{
	for (auto& seat : _seats)
		if (seat.player == player)
			return &seat;
	return nullptr;
}

PokerRoom::Seat* PokerRoom::FindSeatByIndex(int seatIdx)
{
	for (auto& seat : _seats)
		if (seat.seatIndex == seatIdx)
			return &seat;
	return nullptr;
}

void PokerRoom::RemoveEmptySeats()
{
	_seats.erase(std::remove_if(_seats.begin(), _seats.end(), [](const Seat& s)
		{
			return s.player == nullptr || (s.pendingLeave && !s.inHand);
		}), _seats.end());
	if (_seats.empty())
	{
		_button = 0;
		_actingIndex = 0;
	}
	else
	{
		_button %= _seats.size();
		_actingIndex %= _seats.size();
	}
}

int PokerRoom::ActiveSeatCount()
{
	int cnt = 0;
	for (auto& seat : _seats)
		if (seat.player && !seat.pendingLeave && seat.chips > 0)
			cnt++;
	return cnt;
}

void PokerRoom::StartHand()
{
	if (_stage != Stage::Waiting) return;
	if (ActiveSeatCount() < 2) return;
	_pot = 0;
	_stage = Stage::PreFlop;
	_community.clear();
	BuildDeck();
	ShuffleDeck();
	DealHoleCards();
	ResetBets();
	if (!_seats.empty())
		_button = (_button + 1) % _seats.size();
	_actingIndex = NextActiveIndex(_button + 1);
}

void PokerRoom::AdvanceStage()
{
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
	ResetBets();
	_actingIndex = NextActiveIndex(_button + 1);
}

bool PokerRoom::AllBetsMatched()
{
	int active = 0;
	for (auto& seat : _seats)
	{
		if (!seat.player || seat.pendingLeave || !seat.inHand || seat.folded)
			continue;
		if (seat.allIn)
			continue;
		active++;
		if (seat.currentBet != _lastBet)
			return false;
	}
	return active > 0;
}

void PokerRoom::AdvanceTurn()
{
	if (_stage == Stage::Waiting || _seats.empty())
		return;
	if (AllBetsMatched())
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

void PokerRoom::ResolveIfNeeded()
{
	size_t aliveIdx = 0;
	int aliveCnt = 0;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		auto& seat = _seats[i];
		if (!seat.player || seat.pendingLeave || !seat.inHand || seat.folded)
			continue;
		aliveCnt++;
		aliveIdx = i;
	}
	if (aliveCnt == 0)
	{
		FinishHand();
		return;
	}
	if (aliveCnt == 1)
	{
		_seats[aliveIdx].chips += _pot;
		_pot = 0;
		FinishHand();
		return;
	}
	if (_stage == Stage::Showdown)
	{
		HandleShowdown();
		FinishHand();
	}
}

void PokerRoom::FinishHand()
{
	_stage = Stage::Waiting;
	_community.clear();
	_pot = 0;
	_lastBet = 0;
	for (auto& seat : _seats)
	{
		seat.inHand = false;
		seat.folded = false;
		seat.allIn = false;
		seat.currentBet = 0;
		if (seat.pendingLeave)
			seat.player = nullptr;
	}
	RemoveEmptySeats();
}

void PokerRoom::AutoFoldIfNeeded()
{
	if (_stage == Stage::Waiting || _seats.empty())
		return;
	if (_actingIndex >= _seats.size())
		_actingIndex = 0;
	auto& seat = _seats[_actingIndex];
	if (!seat.inHand || seat.folded)
		return;
	if (seat.pendingLeave || seat.player == nullptr || seat.player->Expired())
	{
		seat.folded = true;
		seat.inHand = false;
		ResolveIfNeeded();
		AdvanceTurn();
	}
}

void PokerRoom::HandleShowdown()
{
	if (_pot <= 0)
		return;
	int bestScore = -1;
	std::vector<size_t> winners;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		auto& seat = _seats[i];
		if (!seat.player || seat.pendingLeave || !seat.inHand || seat.folded)
			continue;
		int score = EvaluateHand(seat);
		if (score > bestScore)
		{
			bestScore = score;
			winners.clear();
			winners.push_back(i);
		}
		else if (score == bestScore)
			winners.push_back(i);
	}
	if (winners.empty())
		return;
	int gain = (int)(_pot / (int)winners.size());
	for (auto idx : winners)
		_seats[idx].chips += gain;
	_pot = 0;

	// todo: ????????????????
}

int PokerRoom::EvaluateHand(const Seat& seat)
{
	// todo: ?????????
	return seat.hole[0].rank + seat.hole[1].rank;
}

void PokerRoom::SendTableInfoTo(std::shared_ptr<Player> player)
{
	if (!player || player->Expired()) return;
	NetPack send{ RpcEnum::rpc_client_get_poker_table_info };
	{
		auto rLock = _lock.OnRead();
		send.WriteInt32(_roomId);
		send.WriteUInt8((uint8_t)_stage);
		send.WriteInt32(_pot);
		int actingPlayerId = -1;
		if (_stage != Stage::Waiting && !_seats.empty() && _actingIndex < _seats.size())
		{
			auto& seat = _seats[_actingIndex];
			if (seat.player && !seat.folded && seat.inHand)
				actingPlayerId = seat.player->GetID();
		}
		send.WriteInt32(actingPlayerId);
		send.WriteUInt8((uint8_t)_community.size());
		for (auto& c : _community)
		{
			send.WriteUInt8(c.rank);
			send.WriteUInt8(c.suit);
		}
		send.WriteUInt8((uint8_t)_seats.size());
		for (auto& seat : _seats)
		{
			send.WriteInt32(seat.seatIndex);
			int pid = seat.player ? seat.player->GetID() : -1;
			send.WriteInt32(pid);
			send.WriteInt32(seat.chips);
			send.WriteInt32(seat.currentBet);
			send.WriteUInt8(seat.inHand ? 1 : 0);
			send.WriteUInt8(seat.folded ? 1 : 0);
			send.WriteUInt8(seat.allIn ? 1 : 0);
			send.WriteUInt8(seat.pendingLeave ? 1 : 0);
		}
	}
	player->Send(send);
}

void PokerRoom::BroadcastTableInfo()
{
	std::vector<std::shared_ptr<Player>> members{};
	{
		auto rLock = _lock.OnRead();
		for (const auto& m : _members)
		{
			if (m && !m->Expired())
				members.push_back(m);
		}
	}
	for (auto& p : members)
		SendTableInfoTo(p);
}

void PokerRoom::HandleSitDown(std::shared_ptr<Player> player, int seatIdx)
{
	if (!player) return;
	auto wLock = _lock.OnWrite();
	if (FindSeatByPlayer(player) != nullptr)
		return;
	if (seatIdx < 0)
		seatIdx = 0;
	while (FindSeatByIndex(seatIdx) != nullptr)
		seatIdx++;
	Seat seat{};
	seat.seatIndex = seatIdx;
	seat.player = player;
	_seats.push_back(seat);
	std::sort(_seats.begin(), _seats.end(), [](const Seat& a, const Seat& b)
		{
			return a.seatIndex < b.seatIndex;
		});
	NetPack send{ RpcEnum::rpc_client_sit_down };
	send.WriteInt32(seatIdx);
	send.WriteInt32(seat.chips);
	player->Send(send);
}

void PokerRoom::HandlePlayerAction(std::shared_ptr<Player> player, uint8_t action, int amount)
{
	if (!player) return;
	auto wLock = _lock.OnWrite();
	if (_stage == Stage::Waiting && ActiveSeatCount() >= 2)
		StartHand();
	auto seat = FindSeatByPlayer(player);
	if (seat == nullptr)
		return;
	if (_seats.empty() || _actingIndex >= _seats.size() || _seats[_actingIndex].player != player)
		return;
	if (!seat->inHand || seat->folded || seat->pendingLeave)
		return;

	int toCall = std::max(0, _lastBet - seat->currentBet);
	switch (action)
	{
	case 0: // check / call
	{
		int pay = std::min(seat->chips, toCall);
		seat->chips -= pay;
		seat->currentBet += pay;
		_pot += pay;
		if (seat->chips == 0)
			seat->allIn = true;
		break;
	}
	case 1: // bet / raise
	{
		int pay = std::max(amount, _minBet);
		pay = std::min(pay, seat->chips);
		seat->chips -= pay;
		seat->currentBet += pay;
		_pot += pay;
		_lastBet = seat->currentBet;
		if (seat->chips == 0)
			seat->allIn = true;
		break;
	}
	default: // fold
	{
		seat->folded = true;
		seat->inHand = false;
		break;
	}
	}
	if (seat->pendingLeave && seat->folded)
		seat->player = nullptr;
	ResolveIfNeeded();
	if (_stage != Stage::Waiting)
		AdvanceTurn();
}
