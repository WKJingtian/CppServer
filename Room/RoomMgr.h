#pragma once
#include "Room.h"
#include "Utils/ReadWriteLock.h"
#include "Player/Player.h"
#include "Net/RpcError.h"
#include <thread>
#include <mutex>

class NetPack;
class RoomMgr
{
	static std::unordered_map<int,std::shared_ptr<Room>> _allRoomById;
	static ReadWriteLock _lock;
	static int _roomIdInc;
public:
	static RpcError AddPlayerToRoom(std::shared_ptr<Player> p, int roomId);
	static RpcError RemovePlayerFromRoom(std::shared_ptr<Player> p, int roomId);
	static RpcError CreateRoom(Room::RoomType type, std::shared_ptr<Room>& newRoom);
	static void RemoveRoom(int roomId);
	static void ForEachPlayerInRoom(int roomId, std::function<void(std::shared_ptr<Player>)> func);
	static void WriteAllRoom(NetPack& pack);
	static void WritePlayerRooms(std::shared_ptr<Player> p, NetPack& pack);
	static RpcError HandleNetPack(std::shared_ptr<Player> player, NetPack& pack, int roomId);
	static std::unordered_map<int, std::shared_ptr<Room>> GetAllRoom();
	static void TickAllRoom();
};