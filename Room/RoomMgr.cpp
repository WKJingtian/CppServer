#include "pch.h"
#include "Net/NetPack.h"
#include "RoomMgr.h"
#include "ChatRoom.h"
#include "PokerRoom.h"

std::unordered_map<int, std::shared_ptr<Room>> RoomMgr::_allRoomById =
std::unordered_map<int, std::shared_ptr<Room>>();
ReadWriteLock RoomMgr::_lock = ReadWriteLock();
int RoomMgr::_roomIdInc = 0;

RpcError RoomMgr::AddPlayerToRoom(std::shared_ptr<Player> p, int roomId)
{
	if (p == nullptr) return RpcError::PLAYER_STATE_ERROR;
	
	// Check if already in this room
	if (p->IsInRoom(roomId))
		return RpcError::ALREADY_IN_SELECTED_ROOM;
	
	std::shared_ptr<Room> roomToJoin = nullptr;
	{
		auto rLock = _lock.OnRead();
		if (_allRoomById.contains(roomId))
			roomToJoin = _allRoomById[roomId];
	}
	
	if (roomToJoin == nullptr)
		return RpcError::ROOM_NOT_EXIST;
	
	return roomToJoin->OnPlayerJoin(p);
}
RpcError RoomMgr::RemovePlayerFromRoom(std::shared_ptr<Player> p, int roomId)
{
	if (p == nullptr) return RpcError::PLAYER_STATE_ERROR;
	
	std::shared_ptr<Room> room = nullptr;
	{
		auto rLock = _lock.OnRead();
		if (_allRoomById.contains(roomId))
			room = _allRoomById[roomId];
	}
	
	if (room == nullptr)
		return RpcError::ROOM_NOT_EXIST;
	
	room->OnPlayerExit(p);
	return RpcError::SUCCESS;
}
RpcError RoomMgr::CreateRoom(Room::RoomType type, std::shared_ptr<Room>& newRoom)
{
	int roomId = -1;
	{
		auto wLock = _lock.OnWrite();
		_roomIdInc = (_roomIdInc + 1) % 99999999;
		roomId = _roomIdInc;
	}
	{
		switch (type)
		{
		case Room::RoomType::CHAT_ROOM:
			newRoom = std::make_shared<ChatRoom>();
			break;
		case Room::RoomType::POKER_ROOM:
			newRoom = std::make_shared<PokerRoom>();
			break;
		default:
			return RpcError::ROOM_TYPE_ERROR;
		}
	}
	newRoom->OnRoomCreated(roomId);
	{
		auto wLock = _lock.OnWrite();
		if (_allRoomById.contains(roomId))
			return RpcError::ROOM_ID_OVERFLOW;
		_allRoomById[roomId] = newRoom;
	}
	return RpcError::SUCCESS;
}
void RoomMgr::RemoveRoom(int roomId)
{
	std::shared_ptr<Room> room = nullptr;
	{
		auto wLock = _lock.OnWrite();
		if (!_allRoomById.contains(roomId))
			return;
		room = _allRoomById[roomId];
		_allRoomById.erase(roomId);
	}
	room->OnRoomDestroy();
}
void RoomMgr::ForEachPlayerInRoom(int roomId, std::function<void(std::shared_ptr<Player>)> func)
{
	std::shared_ptr<Room> room = nullptr;
	{
		auto rLock = _lock.OnRead();
		if (!_allRoomById.contains(roomId))
			return;
		room = _allRoomById[roomId];
	}
	room->ForEachPlayerInRoom(func);
}
void RoomMgr::WriteAllRoom(NetPack& pack)
{
	std::unordered_map<int, std::shared_ptr<Room>> mapCopy{};
	{
		auto rLock = _lock.OnRead();
		mapCopy = std::unordered_map<int, std::shared_ptr<Room>>(_allRoomById);
	}
	pack.WriteUInt32((uint32_t)mapCopy.size());
	for (const auto& room : mapCopy)
		room.second->WriteRoom(pack);
}
void RoomMgr::WritePlayerRooms(std::shared_ptr<Player> p, NetPack& pack)
{
	if (p == nullptr)
	{
		pack.WriteUInt32(0);
		return;
	}
	
	auto playerRooms = p->GetRooms();
	std::vector<std::pair<int, Room::RoomType>> roomInfoList;
	
	{
		auto rLock = _lock.OnRead();
		for (int roomId : playerRooms)
		{
			if (_allRoomById.contains(roomId))
			{
				auto& room = _allRoomById[roomId];
				roomInfoList.emplace_back(roomId, room->GetRoomType());
			}
		}
	}
	
	pack.WriteUInt32((uint32_t)roomInfoList.size());
	for (const auto& [roomId, roomType] : roomInfoList)
	{
		pack.WriteInt32(roomId);
		pack.WriteUInt16(roomType);
	}
}
RpcError RoomMgr::HandleNetPack(std::shared_ptr<Player> player, NetPack& pack, int roomId)
{
	if (player == nullptr) return RpcError::PLAYER_STATE_ERROR;
	
	std::shared_ptr<Room> room = nullptr;
	{
		auto rLock = _lock.OnRead();
		if (_allRoomById.contains(roomId))
			room = _allRoomById[roomId];
	}
	
	if (room == nullptr)
		return RpcError::ROOM_NOT_EXIST;
	
	return room->OnRecvPlayerNetPack(player, pack);
}
std::unordered_map<int, std::shared_ptr<Room>> RoomMgr::GetAllRoom()
{
	return _allRoomById;
}
void RoomMgr::TickAllRoom()
{
	std::unordered_map<int, std::shared_ptr<Room>> mapCopy{};
	{
		auto rLock = _lock.OnRead();
		mapCopy = std::unordered_map<int, std::shared_ptr<Room>>(_allRoomById);
	}
	for (const auto& room : mapCopy)
		room.second->OnTick();
}