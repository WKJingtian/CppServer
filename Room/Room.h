#pragma once
#include "Net/RpcError.h"
#include "Utils/ReadWriteLock.h"
#include "Player/Player.h"
#include <thread>
#include <mutex>

// todo: more room type

class NetPack;
class RoomMgr;
class Room
{
public:
	enum RoomType : uint16_t
	{
		HALL = 0,
		CHAT_ROOM = 1,
		POKER_ROOM = 2,
	};
protected:
	std::unordered_set<std::shared_ptr<Player>> _members{};
	ReadWriteLock _lock{};
	RoomType _type;
	int _roomId;
	bool _roomExpired = false;

public:
int GetRoomID() const { return _roomId; }
RoomType GetRoomType() const { return _type; }

virtual void OnRoomCreated(int id);
	virtual void OnRoomDestroy();
	virtual void OnPlayerExit(std::shared_ptr<Player> player);
	virtual RpcError OnRecvPlayerNetPack(std::shared_ptr<Player> player, NetPack& pack);
	virtual void WriteRoom(NetPack& pack);

	virtual bool IsPlayerInRoom(std::shared_ptr<Player> player);
	virtual size_t GetPlayerCnt();
	virtual void ForEachPlayerInRoom(std::function<void(std::shared_ptr<Player>)> func);

	virtual void OnTick();

protected:
	virtual RpcError OnPlayerJoin(std::shared_ptr<Player> player);

	friend RoomMgr;
};