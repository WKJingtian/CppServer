#include "pch.h"
#include "Player.h"
#include "Net/NetPackHandler.h"
#include "Room/RoomMgr.h"


Player::Player(SOCKET&& socket) :
	m_socket(socket)
{
	m_recvThread = std::thread(&Player::RecvJob, this);
}
Player::~Player()
{
	if (m_deleted) return;
	Delete();
}
void Player::RecvJob()
{
	char recvbuf[NET_PACK_MAX_LEN];
	int recvbuflen = NET_PACK_MAX_LEN;
	int iResult;
	// Receive until the peer shuts down the connection
	do {
		iResult = recv(m_socket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			NetPack pack = NetPack((uint8_t*)recvbuf);
			OnRecv(std::move(pack));
		}
		else
			break;
	} while (iResult > 0 && !m_deleted);
	m_recvThread.detach();
}
void Player::OnRecv(NetPack&& pack)
{
	NetPackHandler::AddTask(m_selfPtr, pack);
}
void Player::Send(NetPack& pack)
{
	if (Expired()) return;
	auto iSendResult = send(m_socket, pack.GetContent(), (int)pack.Length(), 0);
	if (iSendResult == SOCKET_ERROR)
		Delete(iSendResult * 100);
}
void Player::Send(RpcEnum msgType, std::function<void(NetPack&)> func)
{
	if (Expired()) return;
	NetPack pack(msgType);
	func(pack);
	Send(pack);
}
void Player::SendError(RpcError err)
{
	if (err != RpcError::SUCCESS)
	{
		NetPack send{ RpcEnum::rpc_client_error_respond };
		send.WriteUInt16(err);
		Send(send);
	}
}
void Player::Delete(int errCode)
{
	if (m_deleted) return;
	m_deleted = true;
	std::cout << "delete player(err " << errCode << ")" << std::endl;
	RoomMgr::AddPlayerToRoom(m_selfPtr, -1);
	m_loggedIn = false;
	m_room = -1;
	m_selfPtr = nullptr;
	if (m_recvThread.joinable()) m_recvThread.join();
	shutdown(m_socket, SD_SEND);
	closesocket(m_socket);
}
bool Player::Expired()
{
	return m_deleted || !m_recvThread.joinable();
}
PlayerInfo& Player::GetInfo()
{
	return m_info;
}
bool Player::IsLoggedIn()
{
	return m_loggedIn;
}
RpcError Player::JoinRoom(int roomIdx)
{
	auto ret = RoomMgr::AddPlayerToRoom(m_selfPtr, roomIdx);
	if (ret == RpcError::SUCCESS) m_room = roomIdx;
	return ret;
}
int Player::GetRoom()
{
	return m_room;
}
int Player::GetID()
{
	return m_info.m_id;
}
std::string Player::GetName()
{
	return m_info.m_name;
}
