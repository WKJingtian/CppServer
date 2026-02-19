#include "pch.h"
#include "AI/HoldemBotConfig.h"
#include "Game/HoldemPokerGame.h"
#include "Net/NetIocpEngine.h"
#include "Net/NetEventBridge.h"

int main(int argc, char** argv)
{
	std::cout << "cpp server project start" << std::endl;

	auto luaConfigModule = LuaConfig::LoadFromExeDir("LuaScript/Config.lua");
	if (!luaConfigModule.Ok())
	{
		std::cerr << "Failed to load config: " << luaConfigModule.error << std::endl;
		return 1;
	}

	auto& configValues = luaConfigModule.values;
	auto requireInt = [&](const char* key, int* outValue) -> bool
	{
		auto it = configValues.find(key);
		if (it == configValues.end())
		{
			std::cerr << "Missing config key: " << key << std::endl;
			return false;
		}
		if (!std::holds_alternative<int>(it->second))
		{
			std::cerr << "Config key must be int: " << key << std::endl;
			return false;
		}
		*outValue = std::get<int>(it->second);
		return true;
	};
	auto requireString = [&](const char* key, std::string* outValue) -> bool
	{
		auto it = configValues.find(key);
		if (it == configValues.end())
		{
			std::cerr << "Missing config key: " << key << std::endl;
			return false;
		}
		if (!std::holds_alternative<std::string>(it->second))
		{
			std::cerr << "Config key must be string: " << key << std::endl;
			return false;
		}
		*outValue = std::get<std::string>(it->second);
		return true;
	};
	auto optionalInt = [&](const char* key, int* outValue) -> bool
	{
		auto it = configValues.find(key);
		if (it == configValues.end())
			return true;
		if (!std::holds_alternative<int>(it->second))
		{
			std::cerr << "Config key must be int: " << key << std::endl;
			return false;
		}
		*outValue = std::get<int>(it->second);
		return true;
	};

	int consoleCodePage = 0;
	int dbPort = 0;
	int netListenPort = 0;
	int netBacklog = 0;
	int iocpWorkerCount = 0;
	int fixedTimeStepMs = 0;
	int netPollIntervalMs = 0;
	int tickLogIntervalMs = 0;
	int timeWheelSlotCount = 0;
	int roomEmptyDestroyDelayMs = 0;
	int pokerMaxSeats = 0;
	int botIdBase = 0;
	int botStartingChips = 0;
	int botThinkTimeMs = 0;
	std::string dbHost;
	std::string dbUser;
	std::string dbPassword;
	std::string dbSchema;
	std::string bindAddr;
	std::string botDefaultName;

	if (!requireInt("console.code_page", &consoleCodePage) ||
		!requireString("db.host", &dbHost) ||
		!requireInt("db.port", &dbPort) ||
		!requireString("db.user", &dbUser) ||
		!requireString("db.password", &dbPassword) ||
		!requireString("db.schema", &dbSchema) ||
		!requireInt("net.listen_port", &netListenPort) ||
		!requireString("net.bind_addr", &bindAddr) ||
		!requireInt("net.backlog", &netBacklog) ||
		!requireInt("loop.fixed_time_step_ms", &fixedTimeStepMs) ||
		!requireInt("loop.net_poll_interval_ms", &netPollIntervalMs) ||
		!requireInt("loop.tick_log_interval", &tickLogIntervalMs) ||
		!requireInt("loop.time_wheel_default_slot_count", &timeWheelSlotCount) ||
		!requireInt("room.empty_destroy_delay_ms", &roomEmptyDestroyDelayMs) ||
		!requireInt("room.poker_max_seats", &pokerMaxSeats) ||
		!requireInt("bot.id_base", &botIdBase) ||
		!requireString("bot.default_name", &botDefaultName) ||
		!requireInt("bot.starting_chips", &botStartingChips) ||
		!requireInt("bot.think_time_ms", &botThinkTimeMs))
	{
		std::cerr << "MISSING CONFIG STIRNG" << std::endl;
		return 1;
	}
	if (!optionalInt("net.iocp_worker_count", &iocpWorkerCount))
		return 1;

	if (consoleCodePage > 0)
	{
		std::string chcpCommand = "chcp " + std::to_string(consoleCodePage);
		system(chcpCommand.c_str());
	}

	auto sqlErrorCode = MySqlMgr::Init(dbHost, dbPort, dbUser, dbPassword, dbSchema);
	if (sqlErrorCode != EXIT_SUCCESS)
		std::cerr << "MySQL init failed!" << std::endl;
	else
		std::cout << "MySQL init succeded!" << std::endl;

	RoomMgr::InitTimerConfig(static_cast<uint32_t>(fixedTimeStepMs),
		static_cast<uint32_t>(timeWheelSlotCount));
	Room::SetEmptyDestroyDelayMs(static_cast<uint32_t>(roomEmptyDestroyDelayMs));
	HoldemBotConfig::Init(botIdBase, botStartingChips, botDefaultName, botThinkTimeMs);
	HoldemPokerGame::SetMaxSeats(pokerMaxSeats);

	WSADATA wsaData;
	int iResult;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	NetIocpEngine netEngine;
	NetEngineConfig netConfig{};
	netConfig.bindAddr = bindAddr;
	netConfig.port = static_cast<uint16_t>(netListenPort);
	netConfig.backlog = netBacklog;
	netConfig.iocpWorkerCount = iocpWorkerCount;
	if (!netEngine.Start(netConfig))
	{
		std::cerr << "NetIocpEngine start failed" << std::endl;
		WSACleanup();
		return 1;
	}

	long long tickClock = tickLogIntervalMs;
	std::vector<NetEvent> netEvents;
	netEvents.reserve(128);
	while (true)
	{
		tickClock += fixedTimeStepMs;
		if (tickClock >= tickLogIntervalMs)
		{
			tickClock -= tickLogIntervalMs;
			std::cout << "[heartbeat] server's main thread is still active!" << std::endl;
		}

		const auto start{ std::chrono::steady_clock::now() };
		long long duration = 0;
		while (duration < fixedTimeStepMs)
		{
			netEvents.clear();
			netEngine.DrainEvents(netEvents);
			NetEventBridge::Dispatch(netEvents, &netEngine);

			auto er = NetPackHandler::DoOneTask();
			while (er != 1)
			{
				if (er != 0) std::cout << "NetPackHandler::DoOneTask WARNING: " << er << std::endl;
				er = NetPackHandler::DoOneTask();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(netPollIntervalMs));
			duration =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		}

		if (duration < fixedTimeStepMs)
			std::this_thread::sleep_for(std::chrono::milliseconds(fixedTimeStepMs - duration));
		std::unordered_set<std::shared_ptr<Player>> pToDelete = std::unordered_set<std::shared_ptr<Player>>();
		PlayerMgr::ForAllPlayer([&pToDelete](auto p)
			{
				if (p->Expired())
					pToDelete.insert(p);
			});

		RoomMgr::TickAllRoom(static_cast<uint32_t>(fixedTimeStepMs));
		PlayerMgr::RemovePlayers(pToDelete);
	}
}
