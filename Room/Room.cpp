#include "pch.h"
#include "Net/NetPack.h"
#include "Room.h"

Room::Room(uint32_t timerTickMs, uint32_t timerSlotCount)
	: _timerWheel(timerTickMs, timerSlotCount)
{
}

uint32_t Room::s_emptyDestroyDelayMs = 0;

void Room::SetEmptyDestroyDelayMs(uint32_t delayMs)
{
	s_emptyDestroyDelayMs = delayMs;
}

void Room::OnRoomCreated(int id)
{
	_roomId = id;
}
void Room::OnRoomDestroy()
{

}
RpcError Room::OnPlayerJoin(std::shared_ptr<Player> player)
{
	auto wLock = _lock.OnWrite();
	if (IsPlayerInRoom(player))
		return RpcError::ALREADY_IN_SELECTED_ROOM;
	if (_roomExpired)
		return RpcError::ROOM_NOT_EXIST;
	_members.insert(player);
	_emptyElapsedMs = 0;
	return RpcError::SUCCESS;
}
void Room::OnPlayerExit(std::shared_ptr<Player> player)
{
	auto wLock = _lock.OnWrite();
	if (!IsPlayerInRoom(player))
		return;
	_members.erase(player);
}
RpcError Room::OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack)
{
	return RpcError::ROOM_TYPE_ERROR;
}
void Room::WriteRoom(NetPack& pack)
{
	std::unordered_set<std::shared_ptr<Player>> members{};
	{
		auto rLock = _lock.OnRead();
		members = std::unordered_set<std::shared_ptr<Player>>(_members);
	}
	pack.WriteInt32(_roomId);
	pack.WriteUInt16(_type);  // Write room type
	pack.WriteUInt32((uint32_t)members.size());
	for (const auto& p : members)
		p->GetInfo().WriteInfo(pack);
}

TimerWheel& Room::GetTimerWheel()
{
	return _timerWheel;
}

const TimerWheel& Room::GetTimerWheel() const
{
	return _timerWheel;
}

bool Room::IsPlayerInRoom(std::shared_ptr<Player> player)
{
	return _members.contains(player);
}
size_t Room::GetPlayerCnt()
{
	return _members.size();
}
void Room::ForEachPlayerInRoom(std::function<void(std::shared_ptr<Player>)> func)
{
	std::unordered_set<std::shared_ptr<Player>> members{};
	{
		auto rLock = _lock.OnRead();
		members = std::unordered_set<std::shared_ptr<Player>>(_members);
	}
	for (auto iter = members.begin(); iter != members.end();)
	{
		auto p = *iter;
		if (p->Expired())
			continue;
		func(p);
		iter++;
	}
}

void Room::Tick(uint32_t elapsedMs)
{
	_timerWheel.AdvanceByElapsedMs(elapsedMs);
	OnTick();
	if (_roomExpired)
		return;

	if (GetPlayerCnt() == 0)
	{
		_emptyElapsedMs += elapsedMs;
		if (s_emptyDestroyDelayMs == 0 || _emptyElapsedMs >= s_emptyDestroyDelayMs)
		{
			_roomExpired = true;
			RoomMgr::RemoveRoom(_roomId);
		}
	}
	else
	{
		_emptyElapsedMs = 0;
	}
}

void Room::OnTick()
{

}
