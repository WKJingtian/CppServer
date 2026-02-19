#include "pch.h"
#include "Player.h"
#include "Net/INetEngine.h"
#include "Room/RoomMgr.h"

Player::Player(NetConnId connId, INetEngine* engine)
	: m_connId(connId), m_netEngine(engine)
{
}
Player::~Player()
{
	if (m_deleted.load()) return;
	Delete();
}
void Player::Send(NetPack& pack)
{
#ifdef ENABLE_PLAYER_CONNECTION_DEBUG
	std::cout << "DEBUG PLAYER ACTION [Send]: " << (int)pack.MsgType() << std::endl;
#endif // ENABLE_PLAYER_CONNECTION_DEBUG
	if (Expired()) return;

	if (m_netEngine)
		m_netEngine->Send(m_connId, reinterpret_cast<const uint8_t*>(pack.GetContent()), pack.Length());
}
void Player::Send(RpcEnum msgType, std::function<void(NetPack&)> func)
{
#ifdef ENABLE_PLAYER_CONNECTION_DEBUG
	std::cout << "DEBUG PLAYER ACTION [Send]: " << (int)msgType << std::endl;
#endif // ENABLE_PLAYER_CONNECTION_DEBUG
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
#ifdef ENABLE_PLAYER_CONNECTION_DEBUG
	std::cout << "DEBUG PLAYER ACTION [Delete]: " << errCode << std::endl;
#endif // ENABLE_PLAYER_CONNECTION_DEBUG

	// Use compare_exchange to ensure only one thread enters
	bool expected = false;
	if (!m_deleted.compare_exchange_strong(expected, true))
		return;

	m_info.WriteInfoToDatabase();
	m_info.WriteAssetToDatabase();
	
	// Leave all rooms before cleanup
	LeaveAllRooms();
	
	m_loggedIn = false;
	m_selfPtr = nullptr;

	if (m_netEngine)
		m_netEngine->Disconnect(m_connId);
}
bool Player::Expired()
{
	return m_deleted.load();
}
NetConnId Player::GetConnId() const
{
	return m_connId;
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
#ifdef ENABLE_PLAYER_CONNECTION_DEBUG
	std::cout << "DEBUG PLAYER ACTION [JoinRoom]: " << roomIdx << std::endl;
#endif // ENABLE_PLAYER_CONNECTION_DEBUG
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
#ifdef ENABLE_PLAYER_CONNECTION_DEBUG
	std::cout << "DEBUG PLAYER ACTION [LeaveRoom]: " << roomIdx << std::endl;
#endif // ENABLE_PLAYER_CONNECTION_DEBUG
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
