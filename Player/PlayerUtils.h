#pragma once
#include <functional>

class Player;

// not thread safe
class PlayerUtils
{
public:

	static void CreateUserOnDatabase(std::string username, std::string password, std::shared_ptr<Player> owner);

	static void UserLogin(int id, std::string password, std::shared_ptr<Player> owner);

	static void FetchUserInfoFromDatabase(std::shared_ptr<Player> owner);

	static void UpdateUserAssetFromDatabase(std::shared_ptr<Player> owner);

	static void WriteUserInfoChangeToDatabase(const PlayerInfo& info);

	static void WriteUserAssetChangeToDatabase(const PlayerInfo& info);

	static void AddChipsToDatabase(int playerId, int delta, std::function<void(bool)> callback);

private:
	static std::string EscapeSqlString(const std::string& input);
};