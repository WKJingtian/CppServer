#pragma once
#include "Card.h"
#include <cstdint>

class NetPack;

struct Seat
{
	int seatIndex = -1;
	int playerId = -1;
	int chips = 0;
	int currentBet = 0;
	int totalBetThisHand = 0;
	bool inHand = false;
	bool folded = false;
	bool allIn = false;
	bool pendingLeave = false;
	bool sittingOut = false;
	bool autoMode = false;
	bool actedThisRound = false;
	Card hole[2]{};

	bool IsOccupied() const { return playerId >= 0 && !pendingLeave; }
	bool CanAct() const { return inHand && !folded && !allIn; }

	void Write(NetPack& pack, bool includeHole = false) const;
	// Read now auto-detects hasHoleCards flag from stream
	void Read(NetPack& pack);
};
