#pragma once
#include "CppServerAPI.h"
#include "NetEventQueue.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct CPPSERVER_API NetEngineConfig
{
	std::string bindAddr;
	uint16_t port = 0;
	int backlog = 0;
	int iocpWorkerCount = 0;
};

class CPPSERVER_API INetEngine
{
public:
	virtual ~INetEngine() = default;

	virtual bool Start(const NetEngineConfig& config) = 0;
	virtual void Stop() = 0;
	virtual void Disconnect(NetConnId connId) = 0;

	// Sends a full frame (including header) to the target connection.
	virtual bool Send(NetConnId connId, const uint8_t* data, size_t len) = 0;

	// Drains all pending events into out. Returns number of drained events.
	virtual size_t DrainEvents(std::vector<NetEvent>& out) = 0;
};
