#pragma once
#include "CppServerAPI.h"
#include "Utils/ReadWriteLock.h"
#include <thread>
#include <mutex>

class NetPack;
class CPPSERVER_API PlayerMgr
{
	static PlayerMgr& Instance();

	std::unordered_set<std::shared_ptr<Player>> _allPlayer;
	std::unordered_set<std::shared_ptr<Player>> _preLogInPlayer;
	std::unordered_map<UINT32, std::shared_ptr<Player>> _loggedInPlayer;
	ReadWriteLock _lock;

	PlayerMgr();
	~PlayerMgr();
	PlayerMgr(const PlayerMgr&) = delete;
	PlayerMgr& operator=(const PlayerMgr&) = delete;

public:
	static std::shared_ptr<Player> OnPlayerConnected(SOCKET&& socket);
	static UINT16 OnPlayerLoggedIn(std::shared_ptr<Player> p, const PlayerInfo& info);
	static void RemovePlayers(std::unordered_set<std::shared_ptr<Player>> playerSet);
	static void ForAllPlayer(std::function<void(std::shared_ptr<Player>)> func);
	static void ForAllLoggedInPlayer(std::function<void(std::shared_ptr<Player>)> func);
	static void ForPlayerWithGivenID(int pid, std::function<void(std::shared_ptr<Player>)> func);
	static void WriteAllPlayer(NetPack& pack);
	static size_t GetLoggedInPlayerCount();
};
