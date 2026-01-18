#include "pch.h"
#include "PlayerMgr.h"
#include "Player.h"

PlayerMgr::PlayerMgr() = default;
PlayerMgr::~PlayerMgr() = default;

PlayerMgr& PlayerMgr::Instance()
{
	static PlayerMgr instance;
	return instance;
}

std::shared_ptr<Player> PlayerMgr::OnPlayerConnected(SOCKET&& socket)
{
	auto& mgr = Instance();
	std::shared_ptr<Player> newPlayer = std::make_shared<Player>(std::move(socket));
	newPlayer->m_selfPtr = newPlayer;
	{
		auto wLock = mgr._lock.OnWrite();
		mgr._allPlayer.insert(newPlayer);
		mgr._preLogInPlayer.insert(newPlayer);
	}
	return newPlayer;
}
UINT16 PlayerMgr::OnPlayerLoggedIn(std::shared_ptr<Player> p, const PlayerInfo& info)
{
	auto& mgr = Instance();
	{
		auto wLock = mgr._lock.OnWrite();
		if (mgr._loggedInPlayer.contains(info.m_id))
			return RpcError::USER_ALREADY_LOGGED_IN_ELSEWHERE;
		mgr._preLogInPlayer.erase(p);
		mgr._loggedInPlayer[info.m_id] = p;
	}
	p->m_info = info;
	p->m_loggedIn = true;
	return RpcError::SUCCESS;
}
void PlayerMgr::RemovePlayers(std::unordered_set<std::shared_ptr<Player>> playerSet)
{
	auto& mgr = Instance();
	{
		auto wLock = mgr._lock.OnWrite();
		for (auto p : playerSet)
		{
			mgr._preLogInPlayer.erase(p);
			mgr._allPlayer.erase(p);
			if (mgr._loggedInPlayer.contains(p->m_info.m_id))
				mgr._loggedInPlayer.erase(p->m_info.m_id);
		}
	}
	for (auto p : playerSet)
		p->Delete();
}
void PlayerMgr::ForAllPlayer(std::function<void(std::shared_ptr<Player>)> func)
{
	auto& mgr = Instance();
	std::unordered_set<std::shared_ptr<Player>> setCopy{};
	{
		auto rLock = mgr._lock.OnRead();
		setCopy = std::unordered_set<std::shared_ptr<Player>>(mgr._allPlayer);
	}
	for (auto p : setCopy)
		func(p);
}
void PlayerMgr::ForAllLoggedInPlayer(std::function<void(std::shared_ptr<Player>)> func)
{
	auto& mgr = Instance();
	std::unordered_map<UINT32, std::shared_ptr<Player>> mapCopy{};
	{
		auto rLock = mgr._lock.OnRead();
		mapCopy = std::unordered_map<UINT32, std::shared_ptr<Player>>(mgr._loggedInPlayer);
	}
	for (auto item : mapCopy)
		func(item.second);
}
void PlayerMgr::ForPlayerWithGivenID(int pid, std::function<void(std::shared_ptr<Player>)> func)
{
	auto& mgr = Instance();
	std::shared_ptr<Player> target = nullptr;
	{
		auto rLock = mgr._lock.OnRead();
		if (!mgr._loggedInPlayer.contains(pid))
			return;
		target = mgr._loggedInPlayer[pid];
	}
	func(target);
}
void PlayerMgr::WriteAllPlayer(NetPack& pack)
{
	auto& mgr = Instance();
	std::unordered_map<UINT32, std::shared_ptr<Player>> mapCopy{};
	{
		auto rLock = mgr._lock.OnRead();
		mapCopy = std::unordered_map<UINT32, std::shared_ptr<Player>>(mgr._loggedInPlayer);
	}
	pack.WriteUInt32((uint32_t)mapCopy.size());
	for (auto item : mapCopy)
		item.second->GetInfo().WriteInfo(pack);
}
size_t PlayerMgr::GetLoggedInPlayerCount()
{
	return Instance()._loggedInPlayer.size();
}