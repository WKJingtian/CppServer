#pragma once
#include "INetEngine.h"
#include "NetIocpContext.h"
#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

class CPPSERVER_API NetIocpEngine final : public INetEngine
{
public:
	NetIocpEngine();
	~NetIocpEngine() override;

	bool Start(const NetEngineConfig& config) override;
	void Stop() override;
	void Disconnect(NetConnId connId) override;
	bool Send(NetConnId connId, const uint8_t* data, size_t len) override;
	size_t DrainEvents(std::vector<NetEvent>& out) override;

private:
	using AcceptExFn = BOOL (PASCAL*)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
	struct AcceptContext
	{
		NetIoOp op;
		SOCKET acceptSocket = INVALID_SOCKET;
	};

	struct RecvContext
	{
		NetIoOp op;
	};

	struct SendContext
	{
		NetIoOp op;
	};

	// IOCP core
	HANDLE _iocpHandle = NULL;
	SOCKET _listenSocket = INVALID_SOCKET;
	std::atomic<bool> _running{false};
	std::vector<std::thread> _workers;
	AcceptExFn _acceptEx = nullptr;
	DWORD _acceptAddrLen = 0;

	// Connection table
	std::mutex _connMutex;
	std::unordered_map<NetConnId, std::shared_ptr<NetConnectionContext>> _connections;
	NetConnId _nextConnId = 1;

	// Event queue (IO thread -> main thread)
	NetEventQueue _eventQueue;

	// Config
	NetEngineConfig _config{};

	// Internal helpers
	bool StartListenSocket();
	bool StartIocpWorkers();
	void StopIocpWorkers();
	void WorkerLoop();

	bool PostAccept(AcceptContext& ctx);
	bool PostRecv(const std::shared_ptr<NetConnectionContext>& ctx, RecvContext& rc);
	bool PostSend(const std::shared_ptr<NetConnectionContext>& ctx, SendContext& sc, std::vector<uint8_t>&& bytes);

	void HandleAccept(AcceptContext* ctx, DWORD bytes);
	void HandleRecv(RecvContext* ctx, DWORD bytes);
	void HandleSend(SendContext* ctx, DWORD bytes);

	std::shared_ptr<NetConnectionContext> FindConnection(NetConnId id);
	void CloseConnection(const std::shared_ptr<NetConnectionContext>& ctx, bool notify);

	void PushConnectedEvent(NetConnId id);
	void PushDisconnectedEvent(NetConnId id);
	void PushFrameEvents(NetConnId id, std::vector<std::vector<uint8_t>>& frames);
};
