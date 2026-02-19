#include "pch.h"
#include "NetIocpEngine.h"
#include <mswsock.h>

NetIocpEngine::NetIocpEngine() = default;
NetIocpEngine::~NetIocpEngine()
{
	Stop();
}

bool NetIocpEngine::Start(const NetEngineConfig& config)
{
	if (_running.load())
		return false;
	_config = config;
	_running.store(true);

	if (!StartListenSocket())
	{
		_running.store(false);
		return false;
	}

	if (!StartIocpWorkers())
	{
		Stop();
		return false;
	}

	// Pre-post a small accept backlog.
	for (int i = 0; i < 4; ++i)
	{
		auto* ctx = new AcceptContext();
		if (!PostAccept(*ctx))
			delete ctx;
	}

	return true;
}

void NetIocpEngine::Stop()
{
	if (!_running.exchange(false))
		return;

	StopIocpWorkers();

	if (_listenSocket != INVALID_SOCKET)
	{
		closesocket(_listenSocket);
		_listenSocket = INVALID_SOCKET;
	}

	if (_iocpHandle)
	{
		CloseHandle(_iocpHandle);
		_iocpHandle = NULL;
	}

	{
		std::lock_guard<std::mutex> lock(_connMutex);
		_connections.clear();
	}
}

void NetIocpEngine::Disconnect(NetConnId connId)
{
	auto ctx = FindConnection(connId);
	if (!ctx)
		return;
	if (ctx->closing.exchange(true))
		return;
	CloseConnection(ctx, false);
}

bool NetIocpEngine::Send(NetConnId connId, const uint8_t* data, size_t len)
{
	auto ctx = FindConnection(connId);
	if (!ctx || ctx->closing.load() || data == nullptr || len == 0)
		return false;

	std::vector<uint8_t> bytes(data, data + len);
	bool needKickoff = false;
	{
		std::lock_guard<std::mutex> lock(ctx->sendMutex);
		needKickoff = ctx->sendQueue.empty() && !ctx->sendInFlight;
		ctx->sendQueue.emplace_back(std::move(bytes));
	}

	if (needKickoff)
	{
		std::vector<uint8_t> first;
		{
			std::lock_guard<std::mutex> lock(ctx->sendMutex);
			if (!ctx->sendQueue.empty())
			{
				first = std::move(ctx->sendQueue.front());
				ctx->sendQueue.pop_front();
				ctx->sendInFlight = true;
			}
		}

		if (!first.empty())
		{
			auto* sc = new SendContext();
			if (!PostSend(ctx, *sc, std::move(first)))
			{
				delete sc;
				CloseConnection(ctx, true);
			}
		}
	}

	return true;
}

size_t NetIocpEngine::DrainEvents(std::vector<NetEvent>& out)
{
	return _eventQueue.Drain(out);
}

bool NetIocpEngine::StartListenSocket()
{
	struct addrinfo* result = nullptr;
	struct addrinfo hints{};
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	const std::string serverPort = std::to_string(_config.port);
	const char* bindAddress = nullptr;
	if (!_config.bindAddr.empty() && _config.bindAddr != "*")
		bindAddress = _config.bindAddr.c_str();

	int iResult = getaddrinfo(bindAddress, serverPort.c_str(), &hints, &result);
	if (iResult != 0)
	{
		std::cout << "getaddrinfo failed: " << iResult << std::endl;
		return false;
	}

	_listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (_listenSocket == INVALID_SOCKET)
	{
		std::cout << "socket failed: " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		return false;
	}

	iResult = bind(_listenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "bind failed: " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		closesocket(_listenSocket);
		_listenSocket = INVALID_SOCKET;
		return false;
	}
	freeaddrinfo(result);

	if (listen(_listenSocket, _config.backlog) == SOCKET_ERROR)
	{
		std::cout << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(_listenSocket);
		_listenSocket = INVALID_SOCKET;
		return false;
	}

	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!_iocpHandle)
	{
		std::cout << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
		closesocket(_listenSocket);
		_listenSocket = INVALID_SOCKET;
		return false;
	}
	if (!CreateIoCompletionPort((HANDLE)_listenSocket, _iocpHandle, 0, 0))
	{
		std::cout << "CreateIoCompletionPort(listen) failed: " << GetLastError() << std::endl;
		closesocket(_listenSocket);
		_listenSocket = INVALID_SOCKET;
		return false;
	}

	GUID guidAcceptEx = WSAID_ACCEPTEX;
	DWORD bytes = 0;
	iResult = WSAIoctl(_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx,
		sizeof(guidAcceptEx),
		&_acceptEx,
		sizeof(_acceptEx),
		&bytes,
		nullptr,
		nullptr);
	if (iResult == SOCKET_ERROR || _acceptEx == nullptr)
	{
		std::cout << "WSAIoctl AcceptEx failed: " << WSAGetLastError() << std::endl;
		return false;
	}

	_acceptAddrLen = sizeof(sockaddr_in) + 16;
	return true;
}

bool NetIocpEngine::StartIocpWorkers()
{
	int workerCount = _config.iocpWorkerCount;
	if (workerCount <= 0)
	{
		SYSTEM_INFO sysInfo{};
		GetSystemInfo(&sysInfo);
		workerCount = (int)sysInfo.dwNumberOfProcessors;
		if (workerCount <= 0)
			workerCount = 1;
	}

	_workers.reserve((size_t)workerCount);
	for (int i = 0; i < workerCount; ++i)
	{
		_workers.emplace_back(&NetIocpEngine::WorkerLoop, this);
	}
	return true;
}

void NetIocpEngine::StopIocpWorkers()
{
	if (_iocpHandle)
	{
		for (size_t i = 0; i < _workers.size(); ++i)
			PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
	}
	for (auto& t : _workers)
	{
		if (t.joinable())
			t.join();
	}
	_workers.clear();
}

void NetIocpEngine::WorkerLoop()
{
	while (_running.load())
	{
		DWORD bytes = 0;
		ULONG_PTR key = 0;
		LPOVERLAPPED overlapped = nullptr;
		BOOL ok = GetQueuedCompletionStatus(_iocpHandle, &bytes, &key, &overlapped, INFINITE);
		if (!_running.load())
			break;
		if (!ok && overlapped == nullptr)
			continue;

		auto* op = reinterpret_cast<NetIoOp*>(overlapped);
		if (!op)
			continue;

		switch (op->type)
		{
		case NetIoOpType::Accept:
			HandleAccept(reinterpret_cast<AcceptContext*>(op), bytes);
			break;
		case NetIoOpType::Recv:
			HandleRecv(reinterpret_cast<RecvContext*>(op), bytes);
			break;
		case NetIoOpType::Send:
			HandleSend(reinterpret_cast<SendContext*>(op), bytes);
			break;
		default:
			break;
		}
	}
}

bool NetIocpEngine::PostAccept(AcceptContext& ctx)
{
	ctx.acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (ctx.acceptSocket == INVALID_SOCKET)
		return false;

	ctx.op.Prepare(NetIoOpType::Accept, _acceptAddrLen * 2);
	DWORD bytes = 0;
	BOOL ok = _acceptEx(_listenSocket, ctx.acceptSocket, ctx.op.buffer.data(), 0,
		_acceptAddrLen, _acceptAddrLen, &bytes, &ctx.op.overlapped);
	if (!ok)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			closesocket(ctx.acceptSocket);
			ctx.acceptSocket = INVALID_SOCKET;
			return false;
		}
	}
	return true;
}

bool NetIocpEngine::PostRecv(const std::shared_ptr<NetConnectionContext>& ctx, RecvContext& rc)
{
	if (!ctx || ctx->closing.load())
		return false;
	rc.op.connId = ctx->connId;
	rc.op.Prepare(NetIoOpType::Recv, NET_PACK_MAX_LEN);
	DWORD flags = 0;
	DWORD bytes = 0;
	int res = WSARecv(ctx->socket, &rc.op.wsaBuf, 1, &bytes, &flags, &rc.op.overlapped, nullptr);
	if (res == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
			return false;
	}
	return true;
}

bool NetIocpEngine::PostSend(const std::shared_ptr<NetConnectionContext>& ctx, SendContext& sc, std::vector<uint8_t>&& bytes)
{
	if (!ctx || ctx->closing.load())
		return false;
	sc.op.connId = ctx->connId;
	sc.op.Prepare(NetIoOpType::Send, bytes.size());
	if (!bytes.empty())
		std::memcpy(sc.op.buffer.data(), bytes.data(), bytes.size());
	DWORD sent = 0;
	int res = WSASend(ctx->socket, &sc.op.wsaBuf, 1, &sent, 0, &sc.op.overlapped, nullptr);
	if (res == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
			return false;
	}
	return true;
}

void NetIocpEngine::HandleAccept(AcceptContext* ctx, DWORD bytes)
{
	(void)bytes;
	if (!ctx || ctx->acceptSocket == INVALID_SOCKET)
		return;

	SOCKET clientSocket = ctx->acceptSocket;
	ctx->acceptSocket = INVALID_SOCKET;

	int opt = 1;
	setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));
	setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(const char*)&_listenSocket, sizeof(_listenSocket));

	if (!CreateIoCompletionPort((HANDLE)clientSocket, _iocpHandle, 0, 0))
	{
		closesocket(clientSocket);
		delete ctx;
		return;
	}

	auto conn = std::make_shared<NetConnectionContext>();
	{
		std::lock_guard<std::mutex> lock(_connMutex);
		conn->connId = _nextConnId++;
		conn->socket = clientSocket;
		_connections[conn->connId] = conn;
	}

	PushConnectedEvent(conn->connId);

	auto* rc = new RecvContext();
	if (!PostRecv(conn, *rc))
	{
		delete rc;
		CloseConnection(conn, true);
	}

	// Re-arm accept
	auto* next = new AcceptContext();
	if (!PostAccept(*next))
		delete next;

	delete ctx;
}

void NetIocpEngine::HandleRecv(RecvContext* ctx, DWORD bytes)
{
	if (!ctx)
		return;
	auto conn = FindConnection(ctx->op.connId);
	if (!conn)
	{
		delete ctx;
		return;
	}

	if (bytes == 0)
	{
		delete ctx;
		CloseConnection(conn, true);
		return;
	}

	std::vector<std::vector<uint8_t>> frames;
	conn->AppendAndExtractFrames(reinterpret_cast<uint8_t*>(ctx->op.buffer.data()), bytes, frames);
	if (!frames.empty())
		PushFrameEvents(conn->connId, frames);
	if (conn->protocolError.load())
	{
		delete ctx;
		CloseConnection(conn, true);
		return;
	}

	if (!PostRecv(conn, *ctx))
	{
		delete ctx;
		CloseConnection(conn, true);
	}
}

void NetIocpEngine::HandleSend(SendContext* ctx, DWORD bytes)
{
	if (!ctx)
		return;
	auto conn = FindConnection(ctx->op.connId);
	if (!conn)
	{
		delete ctx;
		return;
	}
	(void)bytes;

	std::vector<uint8_t> nextBytes;
	{
		std::lock_guard<std::mutex> lock(conn->sendMutex);
		if (!conn->sendQueue.empty())
		{
			nextBytes = std::move(conn->sendQueue.front());
			conn->sendQueue.pop_front();
		}
		else
		{
			conn->sendInFlight = false;
		}
	}

	if (!nextBytes.empty())
	{
		if (!PostSend(conn, *ctx, std::move(nextBytes)))
		{
			delete ctx;
			CloseConnection(conn, true);
			return;
		}
	}
	else
	{
		delete ctx;
	}
}

std::shared_ptr<NetConnectionContext> NetIocpEngine::FindConnection(NetConnId id)
{
	std::lock_guard<std::mutex> lock(_connMutex);
	auto it = _connections.find(id);
	if (it == _connections.end())
		return nullptr;
	return it->second;
}

void NetIocpEngine::CloseConnection(const std::shared_ptr<NetConnectionContext>& ctx, bool notify)
{
	if (!ctx)
		return;
	ctx->closing.store(true);
	if (ctx->socket != INVALID_SOCKET)
	{
		closesocket(ctx->socket);
		ctx->socket = INVALID_SOCKET;
	}
	{
		std::lock_guard<std::mutex> lock(_connMutex);
		_connections.erase(ctx->connId);
	}
	if (notify)
		PushDisconnectedEvent(ctx->connId);
}

void NetIocpEngine::PushConnectedEvent(NetConnId id)
{
	_eventQueue.Push(NetEvent::MakeConnected(id));
}

void NetIocpEngine::PushDisconnectedEvent(NetConnId id)
{
	_eventQueue.Push(NetEvent::MakeDisconnected(id));
}

void NetIocpEngine::PushFrameEvents(NetConnId id, std::vector<std::vector<uint8_t>>& frames)
{
	for (auto& frame : frames)
	{
		_eventQueue.Push(NetEvent::MakeFrame(id, std::move(frame)));
	}
}
