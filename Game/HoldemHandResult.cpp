#include "pch.h"
#include "HoldemHandResult.h"
#include "Net/NetPack.h"

void HandResult::Write(NetPack& pack) const
{
	pack.WriteInt32(totalPot);

	pack.WriteUInt8(static_cast<uint8_t>(communityCards.size()));
	for (const Card& c : communityCards)
		c.Write(pack);

	pack.WriteUInt8(static_cast<uint8_t>(playerResults.size()));
	for (const PlayerHandResult& pr : playerResults)
	{
		pack.WriteInt32(pr.playerId);
		pack.WriteInt32(pr.handRank);
		pack.WriteInt32(pr.chipsWon);
		pack.WriteUInt8(pr.folded ? 1 : 0);
		pr.holeCards[0].Write(pack);
		pr.holeCards[1].Write(pack);
	}
}

void HandResult::Read(NetPack& pack)
{
	Clear();
	totalPot = pack.ReadInt32();

	uint8_t communityCount = pack.ReadUInt8();
	communityCards.reserve(communityCount);
	for (uint8_t i = 0; i < communityCount; ++i)
	{
		Card c;
		c.Read(pack);
		communityCards.push_back(c);
	}

	uint8_t playerCount = pack.ReadUInt8();
	playerResults.reserve(playerCount);
	for (uint8_t i = 0; i < playerCount; ++i)
	{
		PlayerHandResult pr;
		pr.playerId = pack.ReadInt32();
		pr.handRank = pack.ReadInt32();
		pr.chipsWon = pack.ReadInt32();
		pr.folded = pack.ReadUInt8() != 0;
		pr.holeCards[0].Read(pack);
		pr.holeCards[1].Read(pack);
		playerResults.push_back(pr);
	}
}

void HandResult::Clear()
{
	playerResults.clear();
	communityCards.clear();
	totalPot = 0;
}

void HandResult::WriteToDatabase() const
{
}
