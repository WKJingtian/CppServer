#include "pch.h"
#include "NetIocpContext.h"
#include "Net/NetPack.h"
#include <cstring>

void NetIoOp::Prepare(NetIoOpType opType, size_t bufferSize)
{
	type = opType;
	std::memset(&overlapped, 0, sizeof(overlapped));
	buffer.resize(bufferSize);
	wsaBuf.buf = reinterpret_cast<CHAR*>(buffer.data());
	wsaBuf.len = static_cast<ULONG>(buffer.size());
}

size_t NetConnectionContext::AppendAndExtractFrames(const uint8_t* data, size_t len,
	std::vector<std::vector<uint8_t>>& outFrames)
{
	if (protocolError.load() || closing.load())
		return 0;

	if (len > 0)
	{
		pending.insert(pending.end(), data, data + len);
	}

	size_t offset = 0;
	while (pending.size() - offset >= 4)
	{
		uint16_t packetSize = 0;
		std::memcpy(&packetSize, pending.data() + offset + 2, sizeof(packetSize));
		if (packetSize < 4 || packetSize > NET_PACK_MAX_LEN)
		{
			protocolError.store(true);
			break;
		}
		if (pending.size() - offset < packetSize)
			break;

		std::vector<uint8_t> frame(packetSize);
		std::memcpy(frame.data(), pending.data() + offset, packetSize);
		outFrames.emplace_back(std::move(frame));
		offset += packetSize;
	}

	if (offset > 0)
	{
		size_t remaining = pending.size() - offset;
		if (remaining > 0)
			std::memmove(pending.data(), pending.data() + offset, remaining);
		pending.resize(remaining);
	}

	return outFrames.size();
}
