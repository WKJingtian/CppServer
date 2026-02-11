#pragma once
#include "GameItem/Card.h"
#include "GameItem/Deck.h"
#include "GameItem/Seat.h"
#include "HoldemHandResult.h"
#include <vector>
#include <random>
#include <cstdint>

class NetPack;

// Side pot structure for tracking split pots
struct SidePot
{
	int amount = 0;
	std::vector<int> eligiblePlayerIds;
};

class HoldemPokerGame
{
public:
	enum class Stage : uint8_t
	{
		Waiting = 0,
		PreFlop,
		Flop,
		Turn,
		River,
		Showdown
	};

	enum class Action : uint8_t
	{
		CheckCall = 0,
		Bet = 1,
		Raise = 2,
		AllIn = 3,
		Fold = 4
	};

	enum class ActionResult : uint8_t
	{
		Success = 0,
		Ignored = 1,
		Invalid = 2
	};

	enum class BuyInResult : uint8_t
	{
		Success = 0,
		PlayerNotFound = 1,
		BelowMinimum = 2,
		AlreadyInHand = 3
	};

	enum class SetBlindsResult : uint8_t
	{
		Success = 0,
		GameInProgress = 1,
		InvalidValue = 2
	};

	HoldemPokerGame();

	SetBlindsResult SetBlinds(int smallBlind, int bigBlind);
	bool AreBlindsSet() const { return _smallBlind > 0 && _bigBlind > 0; }
	int GetSmallBlind() const { return _smallBlind; }
	int GetBigBlind() const { return _bigBlind; }
	int GetLastBet() const { return _lastBet; }
	int GetLastRaise() const { return _lastRaise; }
	int GetMinBuyin() const { return _minBuyin; }
	int GetDealerSeatIndex() const;
	int GetSmallBlindSeatIndex() const;
	int GetBigBlindSeatIndex() const;

	// Game flow
	bool CanStart() const;
	void StartHand();
	void AdvanceStage();
	void AdvanceTurn();
	ActionResult HandleAction(int playerId, Action action, int amount);
	void ProcessAutoModePlayer();
	void ResolveIfNeeded();
	void FinishHand();

	// Seat management
	bool SitDown(int playerId, int seatIdxHint, int& actualSeatIdx);
	BuyInResult BuyIn(int playerId, int amount);
	bool StandUp(int playerId);
	bool SitBack(int playerId);
	void MarkPendingLeave(int playerId);
	void RemovePendingLeavers();
	int GetPlayerChips(int playerId) const;
	int CashOut(int playerId);

	// Queries
	const Seat* GetSeatByPlayerId(int playerId) const;
	Seat* GetSeatByPlayerId(int playerId);
	const Seat* GetSeatByIndex(int seatIdx) const;
	bool IsSeatActive(const Seat& seat) const;
	int ActiveSeatCount() const;
	int ActingPlayerId() const;
	Stage GetStage() const { return _stage; }
	int GetTotalPot() const;
	const std::vector<Seat>& GetSeats() const { return _seats; }
	const std::vector<Card>& GetCommunity() const { return _community; }
	const std::vector<SidePot>& GetSidePots() const { return _sidePots; }
	
	// Hand result (valid after showdown/single winner)
	const HandResult& GetLastHandResult() const { return _lastHandResult; }
	bool HasPendingHandResult() const { return _hasPendingHandResult; }
	void ClearPendingHandResult() { _hasPendingHandResult = false; }

	// Serialization (for network sync)
	void WriteTable(NetPack& pack, int viewerPlayerId = -1) const;
	void ReadTable(NetPack& pack);

private:
	void DealHoleCards();
	void DealCommunity(size_t count);
	void PostBlinds();
	void CollectBetsToSidePots();
	void ResetBetsForNewRound();
	size_t NextActiveIndex(size_t start, bool includeAllIn = false) const;
	size_t FindNextValidBlindPosition(size_t start) const;
	int InHandSeatCount() const;
	int CalculateDealerSeatIndex() const;
	bool AllBetsMatched() const;
	bool AllActivePlayersActed() const;
	void HandleShowdown();
	void DistributePots();
	int EvaluateHand(const Seat& seat) const;
	void CheckAndSitOutBrokePlayers();
	void RecordHandResult(int totalPot);

	std::vector<Seat> _seats{};
	std::vector<Card> _community{};
	std::vector<SidePot> _sidePots{};
	Deck _deck{};
	std::mt19937 _rng;
	Stage _stage = Stage::Waiting;
	size_t _button = 0;
	size_t _actingIndex = 0;
	int _lastBet = 0;
	int _lastRaise = 0;

	int _smallBlind = -1;
	int _bigBlind = -1;
	int _minBuyin = 1000;
	
	// Hand result tracking
	HandResult _lastHandResult{};
	bool _hasPendingHandResult = false;
};
