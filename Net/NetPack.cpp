#include "pch.h"
#include "NetPack.h"

NetPack::NetPack(RpcEnum typ)
{
	m_enumType = typ;
	m_size = 0;
	assert(m_enumType < RpcEnum::INVALID);
	WriteUInt16((uint16_t)m_enumType);
	WriteUInt16((uint16_t)4);
}
NetPack::NetPack(uint8_t* stream)
{
	std::memcpy(&m_enumType, stream, 2);
	std::memcpy(&m_size, stream + 2, 2);
	assert(m_enumType < RpcEnum::INVALID);
	std::memcpy(m_content, stream, m_size);
	m_readPos = 4;
}
NetPack::NetPack(NetPack&& src) noexcept
{
	std::memmove(&m_enumType, &src.m_enumType, 2);
	std::memmove(&m_size, &src.m_size, sizeof(size_t));
	std::memmove(&m_readPos, &src.m_readPos, sizeof(size_t));
	std::memmove(&m_content, &src.m_content, m_size);
}

void NetPack::operator = (NetPack&& src) noexcept
{
	std::memmove(&m_enumType, &src.m_enumType, 2);
	std::memmove(&m_size, &src.m_size, sizeof(size_t));
	std::memmove(&m_readPos, &src.m_readPos, sizeof(size_t));
	std::memmove(&m_content, &src.m_content, m_size);
}

void NetPack::DebugPrint()
{
	std::cout << "[NETPACK REPORT] typ: " << (uint16_t)m_enumType << "; size: " << m_size << std::endl;
}

//read
float NetPack::ReadFloat()
{
	//assert(m_size >= m_readPos + 4);
	if (m_size < m_readPos + 4) return 0;
	float ret = 0;
	std::memcpy(&ret, m_content + m_readPos, 4);
	m_readPos += 4;
	return ret;
}
std::string NetPack::ReadString()
{
	//assert(m_size >= m_readPos + 2);
	if (m_size < m_readPos + 2) return "";
	uint16_t strlen = ReadUInt16();
	//assert(m_size >= m_readPos + strlen);
	if (m_size < m_readPos + strlen) return "";
	std::string ret((const char*)(m_content + m_readPos), strlen);
	m_readPos += strlen;
	return ret;
}
int8_t NetPack::ReadInt8()
{
	//assert(m_size >= m_readPos + 1);
	if (m_size < m_readPos + 1) return 0;
	int8_t ret = (int8_t)m_content[m_readPos];
	m_readPos += 1;
	return ret;
}
int16_t NetPack::ReadInt16()
{
	//assert(m_size >= m_readPos + 2);
	if (m_size < m_readPos + 2) return 0;
	int16_t ret;
	std::memcpy(&ret, m_content + m_readPos, 2);
	m_readPos += 2;
	return ret;
}
int32_t NetPack::ReadInt32()
{
	//assert(m_size >= m_readPos + 4);
	if (m_size < m_readPos + 4) return 0;
	int32_t ret;
	std::memcpy(&ret, m_content + m_readPos, 4);
	m_readPos += 4;
	return ret;
}
uint8_t NetPack::ReadUInt8()
{
	//assert(m_size >= m_readPos + 1);
	if (m_size < m_readPos + 1) return 0;
	uint8_t ret = m_content[m_readPos];
	m_readPos += 1;
	return ret;
}
uint16_t NetPack::ReadUInt16()
{
	//assert(m_size >= m_readPos + 2);
	if (m_size < m_readPos + 2) return 0;
	uint16_t ret;
	std::memcpy(&ret, m_content + m_readPos, 2);
	m_readPos += 2;
	return ret;
}
uint32_t NetPack::ReadUInt32()
{
	//assert(m_size >= m_readPos + 4);
	if (m_size < m_readPos + 4) return 0;
	uint32_t ret;
	std::memcpy(&ret, m_content + m_readPos, 4);
	m_readPos += 4;
	return ret;
}

//write
void NetPack::WriteFloat(float val, int atPos)
{
	assert(m_size + 4 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 4);
	m_size += 4;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteString(std::string val, int atPos)
{
	uint16_t strlen = (uint16_t)val.length() + 1;
	WriteUInt16(strlen);
	assert(m_size + strlen <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, val.c_str(), strlen);
	m_size += strlen;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteInt8(int8_t val, int atPos)
{
	assert(m_size + 1 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 1);
	m_size += 1;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteInt16(int16_t val, int atPos)
{
	assert(m_size + 2 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 2);
	m_size += 2;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteInt32(int32_t val, int atPos)
{
	assert(m_size + 4 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 4);
	m_size += 4;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteUInt8(uint8_t val, int atPos)
{
	assert(m_size + 1 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 1);
	m_size += 1;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteUInt16(uint16_t val, int atPos)
{
	assert(m_size + 2 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 2);
	m_size += 2;
	std::memcpy(m_content + 2, &m_size, 2);
}
void NetPack::WriteUInt32(uint32_t val, int atPos)
{
	assert(m_size + 4 <= NET_PACK_MAX_LEN);
	std::memcpy(m_content + m_size, &val, 4);
	m_size += 4;
	std::memcpy(m_content + 2, &m_size, 2);
}

const char* NetPack::GetContent() { return (char*)m_content; }
size_t NetPack::Length() { return m_size; }
RpcEnum NetPack::MsgType() { return m_enumType; }