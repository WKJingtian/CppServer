#pragma once
#include "CppServerAPI.h"
#include "Utils/ReadWriteLock.h"
#include "Net/NetEventQueue.h"
#include <mutex>

class NetPack;
class INetEngine;
class CPPSERVER_API PlayerMgr
{
	static PlayerMgr& Instance();

	std::unordered_set<std::shared_ptr<Player>> _allPlayer;
	std::unordered_set<std::shared_ptr<Player>> _preLogInPlayer;
	std::unordered_map<UINT32, std::shared_ptr<Player>> _loggedInPlayer;
	std::unordered_map<NetConnId, std::shared_ptr<Player>> _connIdPlayer;
	ReadWriteLock _lock;

	PlayerMgr();
	~PlayerMgr();
	PlayerMgr(const PlayerMgr&) = delete;
	PlayerMgr& operator=(const PlayerMgr&) = delete;

public:
	static std::shared_ptr<Player> OnPlayerConnected(NetConnId connId, INetEngine* engine);
	static std::shared_ptr<Player> FindByConnId(NetConnId connId);
	static void OnPlayerDisconnected(NetConnId connId);
	static UINT16 OnPlayerLoggedIn(std::shared_ptr<Player> p, const PlayerInfo& info);
	static void RemovePlayers(std::unordered_set<std::shared_ptr<Player>> playerSet);
	static void ForAllPlayer(std::function<void(std::shared_ptr<Player>)> func);
	static void ForAllLoggedInPlayer(std::function<void(std::shared_ptr<Player>)> func);
	static void ForPlayerWithGivenID(int pid, std::function<void(std::shared_ptr<Player>)> func);
	static void WriteAllPlayer(NetPack& pack);
	static size_t GetLoggedInPlayerCount();
};
