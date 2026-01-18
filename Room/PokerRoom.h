#pragma once
# include "Room.h"
#include <vector>
#include <cstdint>

class PokerRoom : public Room
{
	RoomType _type = POKER_ROOM;
public:
	void OnPlayerExit(std::shared_ptr<Player> player) override;
	RpcError OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack) override;
	void OnTick() override;

private:
	struct Card
	{
		uint8_t rank = 0; // 2-14
		uint8_t suit = 0; // 0-3
	};

	struct Seat
	{
		int seatIndex = -1;
		std::shared_ptr<Player> player{};
		int chips = 1000;
		int currentBet = 0;
		bool inHand = false;
		bool folded = false;
		bool allIn = false;
		bool pendingLeave = false;
		Card hole[2]{};
	};

	enum class Stage : uint8_t
	{
		Waiting = 0,
		PreFlop,
		Flop,
		Turn,
		River,
		Showdown
	};

	std::vector<Seat> _seats{};
	std::vector<Card> _community{};
	std::vector<Card> _deck{};
	Stage _stage = Stage::Waiting;
	int _pot = 0;
	size_t _button = 0;
	size_t _actingIndex = 0;
	int _minBet = 10;
	int _lastBet = 0;

	void BuildDeck();
	void ShuffleDeck();
	Card DrawCard();
	void DealHoleCards();
	void DealCommunity(size_t cnt);
	void ResetBets();
	size_t NextActiveIndex(size_t start, bool includeAllIn = false);
	Seat* FindSeatByPlayer(const std::shared_ptr<Player>& player);
	Seat* FindSeatByIndex(int seatIdx);
	void RemoveEmptySeats();
	int ActiveSeatCount();
	void StartHand();
	void AdvanceStage();
	void AdvanceTurn();
	bool AllBetsMatched();
	void ResolveIfNeeded();
	void FinishHand();
	void AutoFoldIfNeeded();
	void HandleShowdown(); // todo: ?????????
	int EvaluateHand(const Seat& seat); // todo: ?????????

	void SendTableInfoTo(std::shared_ptr<Player> player);
	void BroadcastTableInfo();

	void HandleSitDown(std::shared_ptr<Player> player, int seatIdx);
	void HandlePlayerAction(std::shared_ptr<Player> player, uint8_t action, int amount);
};
