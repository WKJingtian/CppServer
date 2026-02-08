#include "pch.h"
#include "HoldemTableSnapshot.h"
#include "Net/NetPack.h"

void HoldemSeatSnapshot::Write(NetPack& pack) const
{
	seat.Write(pack, showHole);
}

void HoldemSeatSnapshot::Read(NetPack& pack)
{
	seat.Read(pack);
	showHole = false;
}

HoldemTableSnapshot HoldemTableSnapshot::Build(const HoldemPokerGame& game, int viewerPlayerId)
{
	HoldemTableSnapshot snapshot{};
	snapshot.stage = game.GetStage();
	snapshot.totalPot = game.GetTotalPot();
	snapshot.actingPlayerId = game.ActingPlayerId();
	snapshot.lastBet = game.GetLastBet();
	snapshot.smallBlind = game.GetSmallBlind();
	snapshot.bigBlind = game.GetBigBlind();
	snapshot.sidePots = game.GetSidePots();
	snapshot.community = game.GetCommunity();

	const auto& seats = game.GetSeats();
	snapshot.seats.reserve(seats.size());
	for (const auto& seat : seats)
	{
		HoldemSeatSnapshot seatSnapshot{};
		seatSnapshot.seat = seat;
		seatSnapshot.showHole = (seat.playerId == viewerPlayerId) ||
			(snapshot.stage == HoldemPokerGame::Stage::Showdown && seat.inHand && !seat.folded);
		snapshot.seats.push_back(seatSnapshot);
	}

	return snapshot;
}

void HoldemTableSnapshot::Write(NetPack& pack) const
{
	pack.WriteUInt8(static_cast<uint8_t>(stage));
	pack.WriteInt32(totalPot);
	pack.WriteInt32(actingPlayerId);
	pack.WriteInt32(lastBet);
	pack.WriteInt32(smallBlind);
	pack.WriteInt32(bigBlind);

	pack.WriteUInt8(static_cast<uint8_t>(sidePots.size()));
	for (const SidePot& pot : sidePots)
	{
		pack.WriteInt32(pot.amount);
		pack.WriteUInt8(static_cast<uint8_t>(pot.eligiblePlayerIds.size()));
		for (int pid : pot.eligiblePlayerIds)
			pack.WriteInt32(pid);
	}

	pack.WriteUInt8(static_cast<uint8_t>(community.size()));
	for (const Card& c : community)
		c.Write(pack);

	pack.WriteUInt8(static_cast<uint8_t>(seats.size()));
	for (const auto& seat : seats)
		seat.Write(pack);
}

void HoldemTableSnapshot::Read(NetPack& pack)
{
	stage = static_cast<HoldemPokerGame::Stage>(pack.ReadUInt8());
	totalPot = pack.ReadInt32();
	actingPlayerId = pack.ReadInt32();
	lastBet = pack.ReadInt32();
	smallBlind = pack.ReadInt32();
	bigBlind = pack.ReadInt32();

	uint8_t potCount = pack.ReadUInt8();
	sidePots.clear();
	sidePots.reserve(potCount);
	for (uint8_t i = 0; i < potCount; ++i)
	{
		SidePot pot;
		pot.amount = pack.ReadInt32();
		uint8_t eligibleCount = pack.ReadUInt8();
		pot.eligiblePlayerIds.reserve(eligibleCount);
		for (uint8_t j = 0; j < eligibleCount; ++j)
			pot.eligiblePlayerIds.push_back(pack.ReadInt32());
		sidePots.push_back(pot);
	}

	uint8_t communityCount = pack.ReadUInt8();
	community.clear();
	community.reserve(communityCount);
	for (uint8_t i = 0; i < communityCount; ++i)
	{
		Card c;
		c.Read(pack);
		community.push_back(c);
	}

	uint8_t seatCount = pack.ReadUInt8();
	seats.clear();
	seats.reserve(seatCount);
	for (uint8_t i = 0; i < seatCount; ++i)
	{
		HoldemSeatSnapshot seatSnapshot;
		seatSnapshot.Read(pack);
		seats.push_back(seatSnapshot);
	}
}
