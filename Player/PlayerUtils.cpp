#include "pch.h"
#include "PlayerUtils.h"
#include "Const.h"

std::string PlayerUtils::EscapeSqlString(const std::string& input)
{
	std::string output;
	output.reserve(input.size() * 2);
	for (char c : input)
	{
		switch (c)
		{
		case '\'': output += "''"; break;  // ?????
		case '\\': output += "\\\\"; break; // ?????
		case '"': output += "\\\""; break;  // ?????
		case '\0': break; // ?????
		default: output += c; break;
		}
	}
	return output;
}

void PlayerUtils::CreateUserOnDatabase(std::string username, std::string password, std::shared_ptr<Player> owner)
{
	int defaultLanguage = (int)Language::English;
	std::string safeUsername = EscapeSqlString(username);
	std::string safePassword = EscapeSqlString(password);
	std::vector<std::string> insertCommand = std::vector<std::string>();
	insertCommand.emplace_back(std::format(CREATE_USER_ACCOUNT_SQL_CMD, safeUsername, safePassword, std::to_string(defaultLanguage)));
	insertCommand.emplace_back(std::format(CREATE_USER_ASSET_SQL_CMD, std::to_string(USER_ACCOUNT_START_CHIP)));
	MySqlMgr::DoSql(insertCommand, [username, owner, defaultLanguage](std::vector<mysqlx::SqlResult>&& resultList)
		{
			// after creating user account, fetch the user info from database
			auto& res = resultList[0];
			auto id = static_cast<uint32_t>(res.getAutoIncrementValue());
			std::string selectCols = "*";
			MySqlMgr::Select("`wkr_server_schema`.`v_user_info_with_asset`", selectCols, "_id=" + std::to_string(id), [owner](mysqlx::SqlResult&& selectRes)
				{
					RpcError err = RpcError::REGISTER_FAILED;
					auto row = selectRes.fetchOne();
					if (!row.isNull())
					{
						PlayerInfo newPlayerInfo = PlayerInfo(row);
						auto logInError = (RpcError)PlayerMgr::OnPlayerLoggedIn(owner, newPlayerInfo);
						err = logInError;
						if (logInError == SUCCESS)
						{
							NetPack send{ RpcEnum::rpc_client_log_in };
							newPlayerInfo.WriteInfo(send);
							owner->Send(send);
						}
					}
					owner->SendError(err);
				});
		}, true);
}

void PlayerUtils::UserLogin(int id, std::string password, std::shared_ptr<Player> owner)
{
	std::erase(password, '\0');
	std::string safePassword = EscapeSqlString(password);
	std::string where = "`_id`=" + std::to_string(id) + " AND `pswd`='" + safePassword + "'";
	std::string selectCols = "_id";
	bool pswdCorrect = false;
	MySqlMgr::Select("`wkr_server_schema`.`user`", selectCols, where, [owner, id, &pswdCorrect](mysqlx::SqlResult&& result)
		{
			auto row = result.fetchOne();
			if (!row.isNull())
			{
				pswdCorrect = true;
			}
			else
				owner->SendError(RpcError::WRONG_PASSWORD);
		});
	if (!pswdCorrect) return;

	MySqlMgr::Select("`wkr_server_schema`.`v_user_info_with_asset`", "*", "`_id`=" + std::to_string(id), [owner, id](mysqlx::SqlResult&& result)
		{
			auto row = result.fetchOne();
			if (!row.isNull())
			{
				PlayerInfo newPlayerInfo = PlayerInfo(row);
				auto logInError = (RpcError)PlayerMgr::OnPlayerLoggedIn(owner, newPlayerInfo);
				if (logInError == SUCCESS)
				{
					NetPack send{ RpcEnum::rpc_client_log_in };
					newPlayerInfo.WriteInfo(send);
					owner->Send(send);
				}
				else
					owner->SendError(logInError);
			}
			else
				owner->SendError(RpcError::SQL_COMMAND_FAILED);
		});
}

void PlayerUtils::FetchUserInfoFromDatabase(std::shared_ptr<Player> owner)
{
	auto id = owner->GetID();
	std::string selectCols = "*";
	MySqlMgr::Select("`wkr_server_schema`.`v_user_info_with_asset`", selectCols, "_id=" + std::to_string(id), [owner](mysqlx::SqlResult&& selectRes)
		{
			auto row = selectRes.fetchOne();
			if (!row.isNull())
			{
				PlayerInfo newPlayerInfo = PlayerInfo(row);
				owner->SetInfo(newPlayerInfo);
				NetPack send{ RpcEnum::rpc_client_refresh_user_info };
				newPlayerInfo.WriteInfo(send);
				owner->Send(send);
			}
			else
				owner->SendError(RpcError::SQL_COMMAND_FAILED);
		});
}

void PlayerUtils::UpdateUserAssetFromDatabase(std::shared_ptr<Player> owner)
{
	auto id = owner->GetID();
	std::string selectCols = "chip";
	MySqlMgr::Select("wkr_server_schema.user_asset", selectCols, "_id=" + std::to_string(id), [owner](mysqlx::SqlResult&& selectRes)
		{
			auto row = selectRes.fetchOne();
			if (!row.isNull())
			{
				owner->GetInfo().SetChipsMemoryOnly(static_cast<int>(row.get(0)));
				NetPack send{ RpcEnum::rpc_client_refresh_user_info };
				owner->GetInfo().WriteInfo(send);
				owner->Send(send);
			}
			else
				owner->SendError(RpcError::SQL_COMMAND_FAILED);
		});
}

void PlayerUtils::WriteUserInfoChangeToDatabase(const PlayerInfo& info)
{
	auto id = info.GetID();
	std::string safeName = EscapeSqlString(info.GetName());
	std::string sqlCmd = std::format("UPDATE wkr_server_schema.user SET _name='{}',lang={} WHERE _id={};",
		safeName, std::to_string((int)info.GetLanguage()), std::to_string(id));
	MySqlMgr::DoSql(sqlCmd, [](mysqlx::SqlResult&& res) {});
}

void PlayerUtils::WriteUserAssetChangeToDatabase(const PlayerInfo& info)
{
	auto id = info.GetID();
	std::string sqlCmd = std::format("UPDATE wkr_server_schema.user_asset SET chip={} WHERE _id={};",
		std::to_string(info.GetChip()), std::to_string(id));
	MySqlMgr::DoSql(sqlCmd, [](mysqlx::SqlResult&& res) {});
}

void PlayerUtils::AddChipsToDatabase(int playerId, int delta, std::function<void(bool)> callback)
{
	std::string sqlCmd;
	if (delta < 0)
	{
		sqlCmd = std::format(
			"UPDATE wkr_server_schema.user_asset SET chip = chip + {} WHERE _id = {} AND chip >= {};",
			delta, playerId, -delta);
	}
	else
	{
		sqlCmd = std::format(
			"UPDATE wkr_server_schema.user_asset SET chip = chip + {} WHERE _id = {};",
			delta, playerId);
	}

	MySqlMgr::DoSql(sqlCmd, [callback, delta](mysqlx::SqlResult&& res)
		{
			auto affectedRows = res.getAffectedItemsCount();
			bool success = (affectedRows > 0);
			if (callback)
				callback(success);
		});
}