#pragma once

#include "GameItem/Card.h"
#include <vector>

class NetPack;

struct PlayerHandResult
{
	int playerId = -1;
	int handRank = 0;
	int chipsWon = 0;
	Card holeCards[2]{};
	bool folded = false;
};

struct HandResult
{
	std::vector<PlayerHandResult> playerResults;
	std::vector<Card> communityCards;
	int totalPot = 0;

	void Write(NetPack& pack) const;
	void Read(NetPack& pack);
	void Clear();
	void WriteToDatabase() const;
};
