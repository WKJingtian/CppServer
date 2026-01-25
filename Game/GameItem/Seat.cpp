#include "pch.h"
#include "Seat.h"
#include "Net/NetPack.h"

void Seat::Write(NetPack& pack, bool includeHole) const
{
	pack.WriteInt32(seatIndex);
	pack.WriteInt32(playerId);
	pack.WriteInt32(chips);
	pack.WriteInt32(currentBet);
	pack.WriteInt32(totalBetThisHand);
	pack.WriteUInt8(inHand ? 1 : 0);
	pack.WriteUInt8(folded ? 1 : 0);
	pack.WriteUInt8(allIn ? 1 : 0);
	pack.WriteUInt8(pendingLeave ? 1 : 0);
	pack.WriteUInt8(sittingOut ? 1 : 0);
	pack.WriteUInt8(autoMode ? 1 : 0);
	
	// Write flag indicating whether hole cards follow
	pack.WriteUInt8(includeHole ? 1 : 0);
	if (includeHole)
	{
		hole[0].Write(pack);
		hole[1].Write(pack);
	}
}

void Seat::Read(NetPack& pack)
{
	seatIndex = pack.ReadInt32();
	playerId = pack.ReadInt32();
	chips = pack.ReadInt32();
	currentBet = pack.ReadInt32();
	totalBetThisHand = pack.ReadInt32();
	inHand = pack.ReadUInt8() != 0;
	folded = pack.ReadUInt8() != 0;
	allIn = pack.ReadUInt8() != 0;
	pendingLeave = pack.ReadUInt8() != 0;
	sittingOut = pack.ReadUInt8() != 0;
	autoMode = pack.ReadUInt8() != 0;
	
	// Read flag and conditionally read hole cards
	bool hasHoleCards = pack.ReadUInt8() != 0;
	if (hasHoleCards)
	{
		hole[0].Read(pack);
		hole[1].Read(pack);
	}
}
