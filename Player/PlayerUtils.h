#pragma once
#include <functional>

class Player;

// not thread safe
static class PlayerUtils
{
public:

	static void CreateUserOnDatabase(std::string username, std::string password, std::shared_ptr<Player> owner);

	static void UserLogin(int id, std::string password, std::shared_ptr<Player> owner);

	static void FetchUserInfoFromDatabase(std::shared_ptr<Player> owner);

	static void UpdateUserAssetFromDatabase(std::shared_ptr<Player> owner);

	static void WriteUserInfoChangeToDatabase(const PlayerInfo& info);

	static void WriteUserAssetChangeToDatabase(const PlayerInfo& info);

	// ????????????????/???
	// delta > 0 ???delta < 0 ??
	// callback(true) ???callback(false) ??????????????
	static void AddChipsToDatabase(int playerId, int delta, std::function<void(bool)> callback);

private:
	// ??SQL?????????
	static std::string EscapeSqlString(const std::string& input);
};