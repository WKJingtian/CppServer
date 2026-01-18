#pragma once
#include "CppServerAPI.h"
#include "Net/RpcError.h"
#include "Net/RpcEnum.h"
#include "PlayerInfo.h"

class NetPack;
class CPPSERVER_API Player
{
	SOCKET m_socket;
	PlayerInfo m_info{-1, "", 0};
	std::thread m_recvThread;
	bool m_deleted = false;
	int m_room = -1;
	bool m_loggedIn = false;
	std::shared_ptr<Player> m_selfPtr = nullptr;
	
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
bool IsLoggedIn();
RpcError JoinRoom(int roomIdx);
	int GetRoom();
	int GetID();
	std::string GetName();

	friend PlayerMgr;
};

