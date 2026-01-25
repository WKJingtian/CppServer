#include "pch.h"

int main(int* args)
{
	std::cout << "cpp server project start" << std::endl;
	system("chcp 936");

	auto sqlErrorCode = MySqlMgr::Init("127.0.0.1", 33060, "root", "1QAZ2wsx", "wkr_server_schema");
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

	const std::string serverPort = "4242";
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, serverPort.c_str(), &hints, &result);
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

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
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
		while (duration < FIXED_TIME_STEP)
		{
			auto er = NetPackHandler::DoOneTask();
			while (er != 1)
			{
				if (er != 0) std::cout << "NetPackHandler::DoOneTask WARNING: " << er << std::endl;
				er = NetPackHandler::DoOneTask();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			duration =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		}
		if (duration < FIXED_TIME_STEP)
			std::this_thread::sleep_for(std::chrono::milliseconds(FIXED_TIME_STEP - duration));
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