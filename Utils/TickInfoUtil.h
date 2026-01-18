#pragma once
#include <utility>
#include <functional>
#include "Net/NetPack.h"

static class TickInfoUtil
{
public:
	enum TickInfoType : uint16_t
	{
		TICK_NOTHING = 0,
		CHAT_ROOM_TICK = 1,
		POKER_ROOM_TICK = 2,
	};

	static void ConstructTickInfo(NetPack& pack, TickInfoType type, std::function<void(NetPack&)> func)
	{
		pack.WriteUInt16((uint16_t)type);
		func(pack);
	}
};