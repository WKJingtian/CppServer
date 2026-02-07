#include "pch.h"

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

	int consoleCodePage = 0;
	int dbPort = 0;
	int netListenPort = 0;
	int netBacklog = 0;
	int fixedTimeStepMs = 0;
	int netPollIntervalMs = 0;
	std::string dbHost;
	std::string dbUser;
	std::string dbPassword;
	std::string dbSchema;
	std::string bindAddr;

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
		!requireInt("loop.net_poll_interval_ms", &netPollIntervalMs))
	{
		std::cerr << "MISSING CONFIG STIRNG" << std::endl;
		return 1;
	}

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

	WSADATA wsaData;
	int iResult;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	const std::string serverPort = std::to_string(netListenPort);
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	// Resolve the local address and port to be used by the server
	const char* bindAddress = nullptr;
	if (!bindAddr.empty() && bindAddr != "*")
		bindAddress = bindAddr.c_str();
	iResult = getaddrinfo(bindAddress, serverPort.c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET ListenSocket = INVALID_SOCKET;
	// Create a SOCKET for the server to listen for client connections
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result);

	if (listen(ListenSocket, netBacklog) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	std::function<int(void)> listenJob = [ListenSocket]() -> int
	{
		SOCKET clientSocket = INVALID_SOCKET;
		// Accept a client socket
		clientSocket = accept(ListenSocket, NULL, NULL);
		std::vector<std::thread> clientThreads{};
		while (clientSocket != INVALID_SOCKET)
		{
			PlayerMgr::OnPlayerConnected(std::move(clientSocket));
			clientSocket = accept(ListenSocket, NULL, NULL);
		}
		printf("accept failed: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		for (auto& t : clientThreads) t.join();
		return 1;
	};
	std::thread netThread = std::thread(listenJob);

	while (true)
	{
		const auto start{ std::chrono::steady_clock::now() };
		long long duration = 0;
		while (duration < fixedTimeStepMs)
		{
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
				else if (p)
				{
					if (p->GetRooms().empty())
					{
						NetPack tickPack{ RpcEnum::rpc_server_tick };
						TickInfoUtil::ConstructTickInfo(tickPack, TickInfoUtil::TICK_NOTHING, [](NetPack& pack) {});
						p->Send(tickPack);
					}
				}
			});

		RoomMgr::TickAllRoom();
		PlayerMgr::RemovePlayers(pToDelete);
	}
}
