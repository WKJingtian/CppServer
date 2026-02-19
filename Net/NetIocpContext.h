#pragma once
#include "CppServerAPI.h"
#include "NetEventQueue.h"
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

enum class NetIoOpType : uint8_t
{
	Accept = 0,
	Recv = 1,
	Send = 2
};

struct CPPSERVER_API NetIoOp
{
	OVERLAPPED overlapped{};
	NetIoOpType type = NetIoOpType::Recv;
	WSABUF wsaBuf{};
	std::vector<uint8_t> buffer;
	NetConnId connId = 0;

	void Prepare(NetIoOpType opType, size_t bufferSize);
};

struct CPPSERVER_API NetConnectionContext
{
	NetConnId connId = 0;
	SOCKET socket = INVALID_SOCKET;

	// Pending bytes for TCP framing (per-connection).
	std::vector<uint8_t> pending;

	// Send queue and state (filled by main thread, drained by IO thread).
	std::mutex sendMutex;
	std::deque<std::vector<uint8_t>> sendQueue;
	bool sendInFlight = false;

	std::atomic<bool> closing{false};
	std::atomic<bool> protocolError{false};

	// Append raw bytes and extract complete frames (including header).
	// Returns the number of frames extracted into outFrames.
	size_t AppendAndExtractFrames(const uint8_t* data, size_t len,
		std::vector<std::vector<uint8_t>>& outFrames);
};
