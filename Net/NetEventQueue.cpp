#include "pch.h"
#include "NetEventQueue.h"

NetEvent NetEvent::MakeConnected(NetConnId id)
{
	NetEvent ev;
	ev.type = NetEventType::Connected;
	ev.connId = id;
	return ev;
}

NetEvent NetEvent::MakeDisconnected(NetConnId id)
{
	NetEvent ev;
	ev.type = NetEventType::Disconnected;
	ev.connId = id;
	return ev;
}

NetEvent NetEvent::MakeFrame(NetConnId id, std::vector<uint8_t>&& bytes)
{
	NetEvent ev;
	ev.type = NetEventType::Frame;
	ev.connId = id;
	ev.data = std::move(bytes);
	return ev;
}

void NetEventQueue::Push(NetEvent&& ev)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_queue.emplace(std::move(ev));
}

bool NetEventQueue::TryPop(NetEvent& out)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_queue.empty())
		return false;
	out = std::move(_queue.front());
	_queue.pop();
	return true;
}

size_t NetEventQueue::Drain(std::vector<NetEvent>& out)
{
	std::queue<NetEvent> local;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		std::swap(local, _queue);
	}

	size_t count = 0;
	while (!local.empty())
	{
		out.emplace_back(std::move(local.front()));
		local.pop();
		++count;
	}
	return count;
}
