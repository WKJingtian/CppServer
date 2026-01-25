#pragma once
#include "CppServerAPI.h"
#include "Net/RpcError.h"
#include "Net/RpcEnum.h"
#include "PlayerInfo.h"
#include <mutex>
#include <atomic>
#include <unordered_set>

class NetPack;
class CPPSERVER_API Player
{
	SOCKET m_socket;
	PlayerInfo m_info{};
	std::thread m_recvThread;
	std::atomic<bool> m_deleted{false};
	
	// Multi-room support: set of room IDs the player is in
	std::unordered_set<int> m_rooms{};
	mutable std::mutex m_roomsMutex;
	
	bool m_loggedIn = false;
	std::shared_ptr<Player> m_selfPtr = nullptr;
	
	// Mutex for protecting socket send operations
	mutable std::mutex m_sendMutex;
	
	void RecvJob();
	void OnRecv(NetPack&& pack);
public:
	Player() = delete;
	Player(SOCKET&& socket);
	~Player();
	void Send(NetPack& pack);
	void Send(RpcEnum msgType, std::function<void(NetPack&)> func);
	void SendError(RpcError err);
	void Delete(int errCode = 0);
	bool Expired();

	PlayerInfo& GetInfo();
	void SetInfo(PlayerInfo newInfo);

	bool IsLoggedIn();
	
	// Multi-room support methods
	RpcError JoinRoom(int roomIdx);
	RpcError LeaveRoom(int roomIdx);
	void LeaveAllRooms();
	std::unordered_set<int> GetRooms();
	bool IsInRoom(int roomIdx);
	
	int GetID();
	std::string GetName();

	friend PlayerMgr;
};
