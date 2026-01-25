#include "pch.h"
#include "Card.h"
#include "Net/NetPack.h"

void Card::Write(NetPack& pack) const
{
	pack.WriteUInt8(_rank);
	pack.WriteUInt8(_suit);
}

void Card::Read(NetPack& pack)
{
	_rank = pack.ReadUInt8();
	_suit = pack.ReadUInt8();
}
