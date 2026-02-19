#pragma once
#include "CppServerAPI.h"
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

using NetConnId = uint64_t;

enum class NetEventType : uint8_t
{
	Connected = 0,
	Disconnected = 1,
	Frame = 2
};

struct CPPSERVER_API NetEvent
{
	NetEventType type = NetEventType::Frame;
	NetConnId connId = 0;
	// Payload is copied for safety and simplicity.
	// TODO: optimize with a buffer pool or ref-counted slices to reduce copying.
	// TODO: consider a lock-free MPSC queue if this becomes a bottleneck.
	std::vector<uint8_t> data;

	static NetEvent MakeConnected(NetConnId id);
	static NetEvent MakeDisconnected(NetConnId id);
	static NetEvent MakeFrame(NetConnId id, std::vector<uint8_t>&& bytes);
};

class CPPSERVER_API NetEventQueue
{
public:
	void Push(NetEvent&& ev);
	bool TryPop(NetEvent& out);
	size_t Drain(std::vector<NetEvent>& out);

private:
	mutable std::mutex _mutex;
	std::queue<NetEvent> _queue;
};
