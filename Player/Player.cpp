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
	if (m_deleted.load()) return;
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
	} while (iResult > 0 && !m_deleted.load());
	m_recvThread.detach();
}
void Player::OnRecv(NetPack&& pack)
{
	NetPackHandler::AddTask(m_selfPtr, pack);
}
void Player::Send(NetPack& pack)
{
	if (Expired()) return;
	
	std::lock_guard<std::mutex> lock(m_sendMutex);
	// Double-check after acquiring lock
	if (m_deleted.load()) return;
	
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
	// Use compare_exchange to ensure only one thread enters
	bool expected = false;
	if (!m_deleted.compare_exchange_strong(expected, true))
		return;
	
	std::cout << "delete player(err " << errCode << ")" << std::endl;
	m_info.WriteInfoToDatabase();
	m_info.WriteAssetToDatabase();
	
	// Leave all rooms before cleanup
	LeaveAllRooms();
	
	m_loggedIn = false;
	m_selfPtr = nullptr;
	
	// Lock to ensure no send operations are in progress
	{
		std::lock_guard<std::mutex> lock(m_sendMutex);
		shutdown(m_socket, SD_SEND);
		closesocket(m_socket);
	}
	
	if (m_recvThread.joinable()) m_recvThread.join();
}
bool Player::Expired()
{
	return m_deleted.load() || !m_recvThread.joinable();
}
PlayerInfo& Player::GetInfo()
{
	return m_info;
}
void Player::SetInfo(PlayerInfo newInfo)
{
	m_info = newInfo;
}
bool Player::IsLoggedIn()
{
	return m_loggedIn;
}
RpcError Player::JoinRoom(int roomIdx)
{
	auto ret = RoomMgr::AddPlayerToRoom(m_selfPtr, roomIdx);
	if (ret == RpcError::SUCCESS)
	{
		std::lock_guard<std::mutex> lock(m_roomsMutex);
		m_rooms.insert(roomIdx);
	}
	return ret;
}
RpcError Player::LeaveRoom(int roomIdx)
{
	auto ret = RoomMgr::RemovePlayerFromRoom(m_selfPtr, roomIdx);
	if (ret == RpcError::SUCCESS)
	{
		std::lock_guard<std::mutex> lock(m_roomsMutex);
		m_rooms.erase(roomIdx);
	}
	return ret;
}
void Player::LeaveAllRooms()
{
	std::unordered_set<int> roomsCopy;
	{
		std::lock_guard<std::mutex> lock(m_roomsMutex);
		roomsCopy = m_rooms;
	}
	for (int roomId : roomsCopy)
		LeaveRoom(roomId);
}
std::unordered_set<int> Player::GetRooms()
{
	std::lock_guard<std::mutex> lock(m_roomsMutex);
	return m_rooms;
}
bool Player::IsInRoom(int roomIdx)
{
	std::lock_guard<std::mutex> lock(m_roomsMutex);
	return m_rooms.contains(roomIdx);
}
int Player::GetID()
{
	return m_info.GetID();
}
std::string Player::GetName()
{
	return m_info.GetName();
}
