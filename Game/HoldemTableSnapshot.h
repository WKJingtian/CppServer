#pragma once

#include "GameItem/Card.h"
#include "GameItem/Seat.h"
#include "HoldemPokerGame.h"
#include <vector>

class NetPack;

struct HoldemSeatSnapshot
{
	Seat seat{};
	bool showHole = false;

	void Write(NetPack& pack) const;
	void Read(NetPack& pack);
};

struct HoldemTableSnapshot
{
	HoldemPokerGame::Stage stage = HoldemPokerGame::Stage::Waiting;
	int totalPot = 0;
	int actingPlayerId = -1;
	int lastBet = 0;
	int smallBlind = 0;
	int bigBlind = 0;
	std::vector<SidePot> sidePots{};
	std::vector<Card> community{};
	std::vector<HoldemSeatSnapshot> seats{};

	static HoldemTableSnapshot Build(const HoldemPokerGame& game, int viewerPlayerId);
	void Write(NetPack& pack) const;
	void Read(NetPack& pack);
};
