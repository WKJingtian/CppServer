#pragma once
#include "CppServerAPI.h"
#include "RpcEnum.h"

#define NET_PACK_MAX_LEN 4096

class CPPSERVER_API NetPack
{
	RpcEnum m_enumType = RpcEnum::INVALID;
	size_t m_readPos = 0;
	size_t m_size = 0;
	uint8_t m_content[NET_PACK_MAX_LEN];
public:
	NetPack() = delete;
	NetPack(RpcEnum typ);               // used to write & send
	NetPack(uint8_t* stream);           // used to receive & read
	NetPack(NetPack&& src) noexcept;    // move to different thread

	void operator = (NetPack&& src) noexcept;

	const char* GetContent();
	size_t Length();
	RpcEnum MsgType();

	//read
	float ReadFloat();
	std::string ReadString();
	int8_t ReadInt8();
	int16_t ReadInt16();
	int32_t ReadInt32();
	uint8_t ReadUInt8();
	uint16_t ReadUInt16();
	uint32_t ReadUInt32();

	//write
	void WriteFloat(float val, int atPos = -1);
	void WriteString(std::string val, int atPos = -1);
	void WriteInt8(int8_t val, int atPos = -1);
	void WriteInt16(int16_t val, int atPos = -1);
	void WriteInt32(int32_t val, int atPos = -1);
	void WriteUInt8(uint8_t val, int atPos = -1);
	void WriteUInt16(uint16_t val, int atPos = -1);
	void WriteUInt32(uint32_t val, int atPos = -1);

	void DebugPrint();
	uint8_t* DebugGetContent() { return (uint8_t*)m_content; }
};

