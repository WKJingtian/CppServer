#include "pch.h"
#include "NetPackHandler.h"

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
		std::erase(pwd, '\0');
		std::string where = "_id=" + std::to_string(id) + " AND pswd=\"" + pwd + "\"";
		std::string selectCols = "_name, lang";
		MySqlMgr::Select("wkr_server_schema.user", selectCols, where, [&owner, id](mysqlx::SqlResult&& result)
			{
				auto row = result.fetchOne();
				if (!row.isNull())
				{
					auto nickname = row.get(0).get<std::string>();
					auto language = row.get(1).get<int>();
					auto error = (RpcError)PlayerMgr::OnPlayerLoggedIn(owner, PlayerInfo(id, nickname, language));
					if (error == SUCCESS)
					{
						NetPack send{ RpcEnum::rpc_client_log_in };
						send.WriteUInt32(id);
						send.WriteString(nickname);
						send.WriteUInt32(language);
						owner->Send(send);
					}
					else
						owner->SendError(error);
				}
				else
					owner->SendError(RpcError::WRONG_PASSWORD);
			});
	}
	else if (pack.MsgType() == RpcEnum::rpc_server_register)
	{
		if (owner->IsLoggedIn())
			owner->SendError(RpcError::USER_ALREADY_LOGGED_IN);
		std::string name = pack.ReadString();
		std::string pwd = pack.ReadString();
		std::string values = "\"" + name + "\", \"" + pwd + "\", 0";
		std::string insertCols = "_name, pswd, lang";
		MySqlMgr::Upsert("user", insertCols, values, "", [&owner, name](mysqlx::SqlResult&& res)
			{
				auto affected = res.getAffectedItemsCount();
				if (affected == 0)
				{
					owner->SendError(RpcError::REGISTER_FAILED);
					return;
				}
				auto id = static_cast<uint32_t>(res.getAutoIncrementValue());
				if (id == 0)
				{
					owner->SendError(RpcError::REGISTER_FAILED);
					return;
				}
				std::string selectCols = "*";
				MySqlMgr::Select("user", selectCols, "_id=" + std::to_string(id), [&owner, name, id](mysqlx::SqlResult&& result)
					{
						RpcError err = RpcError::REGISTER_FAILED;
						auto row = result.fetchOne();
						if (!row.isNull())
						{
							auto error = (RpcError)PlayerMgr::OnPlayerLoggedIn(owner, PlayerInfo(id, name, 0));
							err = error;
							if (error == SUCCESS)
							{
								NetPack send{ RpcEnum::rpc_client_log_in };
								send.WriteUInt32(id);
								send.WriteString(name);
								send.WriteUInt32(0);
								owner->Send(send);
							}
						}
						owner->SendError(err);
					});
			});
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
	// todo: push change to database
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
		|| pack.MsgType() == RpcEnum::rpc_server_poker_action)
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