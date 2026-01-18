#pragma once
#include "CppServerAPI.h"
#include "Player/Player.h"
#include "Room/RoomMgr.h"

struct CPPSERVER_API NetTask
{
	std::shared_ptr<Player> m_taskOwner;
	NetPack m_taskPack;
	NetTask(std::shared_ptr<Player> owner, NetPack& pack);
	NetTask(NetTask&& other) noexcept;
	~NetTask();
};

class CPPSERVER_API NetPackHandler
{
	static NetPackHandler& Instance();

	std::queue<NetTask> _taskList;
	std::mutex _mutex;

	NetPackHandler();
	~NetPackHandler();
	NetPackHandler(const NetPackHandler&) = delete;
	NetPackHandler& operator=(const NetPackHandler&) = delete;

public:
	static void AddTask(std::shared_ptr<Player> owner, NetPack& pack);
	static int DoOneTask();
};
