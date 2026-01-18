#include "pch.h"
#include "Net/NetPack.h"
#include "Room.h"

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
	pack.WriteUInt32((uint32_t)members.size());
	for (const auto& p : members)
		p->GetInfo().WriteInfo(pack);
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

void Room::OnTick()
{

}