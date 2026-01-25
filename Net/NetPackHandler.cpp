#include "pch.h"
#include "NetPackHandler.h"
#include "Player/PlayerUtils.h"

NetTask::NetTask(std::shared_ptr<Player> owner, NetPack& pack)
	: m_taskOwner(owner), m_taskPack(std::move(pack))
{ }

NetTask::NetTask(NetTask&& other) noexcept
	: m_taskOwner(std::move(other.m_taskOwner)), m_taskPack(std::move(other.m_taskPack))
{ }

NetTask::~NetTask() = default;

NetPackHandler::NetPackHandler() = default;
NetPackHandler::~NetPackHandler() = default;

NetPackHandler& NetPackHandler::Instance()
{
	static NetPackHandler instance;
	return instance;
}

void NetPackHandler::AddTask(std::shared_ptr<Player> owner, NetPack& pack)
{
	auto& handler = Instance();
	std::unique_lock<std::mutex> lock(handler._mutex);
	handler._taskList.push(NetTask(owner, pack));
}
int NetPackHandler::DoOneTask()
{
	auto& handler = Instance();
	std::shared_ptr<Player> owner = nullptr;
	NetPack pack{ RpcEnum::INVALID };

	{
		std::unique_lock<std::mutex> lock(handler._mutex);
		if (handler._taskList.size() == 0)
			return 1; // no task to do
		NetTask& task = handler._taskList.front();
		handler._taskList.pop();
		owner = task.m_taskOwner;
		pack = std::move(task.m_taskPack);
	}

	if (owner == nullptr || owner->Expired())
		return 2; // owner no longer valid

	if (pack.MsgType() == RpcEnum::rpc_server_log_in)
	{
		if (owner->IsLoggedIn())
			owner->SendError(RpcError::USER_ALREADY_LOGGED_IN);
		UINT32 id = pack.ReadUInt32();
		std::string pwd = pack.ReadString();
		PlayerUtils::UserLogin(id, pwd, owner);
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_register)
	{
		if (owner->IsLoggedIn())
			owner->SendError(RpcError::USER_ALREADY_LOGGED_IN);
		std::string name = pack.ReadString();
		std::string pwd = pack.ReadString();
		PlayerUtils::CreateUserOnDatabase(name, pwd, owner);
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_print_room)
	{
		NetPack send{ RpcEnum::rpc_client_print_room };
		RoomMgr::WriteAllRoom(send);
		owner->Send(send);
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_print_user)
	{
		NetPack send{ RpcEnum::rpc_client_print_user };
		PlayerMgr::WriteAllPlayer(send);
		owner->Send(send);
	}
	else if (!owner->IsLoggedIn())
	{
		owner->SendError(RpcError::NOT_LOGGED_IN);
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_set_name)
	{
		owner->GetInfo().SetName(pack.ReadString());
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_set_language)
	{
		auto lang = pack.ReadString();
		if (lang.rfind("en", 0) == 0)
			owner->GetInfo().SetLanguage(Language::English);
		else if (lang.rfind("cn", 0) == 0)
			owner->GetInfo().SetLanguage(Language::Chinese);
		else {}
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_goto_room)
	{
		int roomIdx = pack.ReadInt32();
		auto err = owner->JoinRoom(roomIdx);
		if (err != SUCCESS)
			owner->SendError(err);
		else
		{
			NetPack send{ RpcEnum::rpc_client_goto_room };
			send.WriteInt32(roomIdx);
			owner->Send(send);
		}
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_create_room)
	{
		std::shared_ptr<Room> newRoom = nullptr;
		int roomType = pack.ReadUInt16();
		auto err = RoomMgr::CreateRoom((Room::RoomType)roomType, newRoom);
		if (err != SUCCESS)
			owner->SendError(err);
		else
		{
			NetPack send{ RpcEnum::rpc_client_create_room };
			send.WriteInt32(newRoom->GetRoomID());
			owner->Send(send);
		}
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_send_text
		|| pack.MsgType() == RpcEnum::rpc_server_get_poker_table_info
		|| pack.MsgType() == RpcEnum::rpc_server_sit_down
		|| pack.MsgType() == RpcEnum::rpc_server_poker_action
		|| pack.MsgType() == RpcEnum::rpc_server_poker_buyin
		|| pack.MsgType() == RpcEnum::rpc_server_poker_standup
		|| pack.MsgType() == RpcEnum::rpc_server_poker_set_blinds)
	{
		auto err = RoomMgr::HandleNetPack(owner, pack);
		owner->SendError(err);
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_error_respond)
	{
		std::cout << "Client Give Error Respond" << std::endl;
		if (owner->IsLoggedIn())
			std::cout << "\tFrom: " << owner->GetName() << " " << owner->GetID() << std::endl;
		std::cout << "\t" << pack.ReadUInt16() << std::endl;
	}
	else
	{
		owner->SendError(RpcError::UNKNOWN_RPC_ERROR);
		std::cout << "Client Sends Error RPC" << std::endl;
		if (owner->IsLoggedIn())
			std::cout << "\tFrom: " << owner->GetName() << " " << owner->GetID() << std::endl;
		std::cout << "\t" << (uint16_t)pack.MsgType() << std::endl;
	}

	return 0;
}