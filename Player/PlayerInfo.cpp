#include "pch.h"
#include "PlayerInfo.h"
#include "PlayerUtils.h"
#include "Const.h"

PlayerInfo::PlayerInfo()
	: m_id(-1), m_name(""), m_language(Language::English), m_chipCount(0)
{
}

PlayerInfo::PlayerInfo(const PlayerInfo& other)
{
	auto rLock = other.m_lock.OnRead();
	m_id = other.m_id;
	m_name = other.m_name;
	m_language = other.m_language;
	m_chipCount.store(other.m_chipCount.load());
}

PlayerInfo& PlayerInfo::operator=(const PlayerInfo& other)
{
	if (this != &other)
	{
		auto rLock = other.m_lock.OnRead();
		auto wLock = m_lock.OnWrite();
		m_id = other.m_id;
		m_name = other.m_name;
		m_language = other.m_language;
		m_chipCount.store(other.m_chipCount.load());
	}
	return *this;
}

PlayerInfo::PlayerInfo(NetPack& src)
	: m_chipCount(0)
{
	ReadInfo(src);
}

PlayerInfo::~PlayerInfo()
{
}

void PlayerInfo::SetName(std::string n)
{
	auto wLock = m_lock.OnWrite();
	m_name = n;
}

void PlayerInfo::SetLanguage(Language l)
{
	auto wLock = m_lock.OnWrite();
	m_language = l;
}

void PlayerInfo::AddChipsMemoryOnly(int delta)
{
	m_chipCount.fetch_add(delta);
}

void PlayerInfo::SetChipsMemoryOnly(int newCount)
{
	m_chipCount.store(newCount);
}

void PlayerInfo::AddChips(int addCount)
{
	m_chipCount.fetch_add(addCount);
#ifdef IS_CPP_SERVER
	WriteAssetToDatabase();
#endif
}

void PlayerInfo::SetChips(int newCount)
{
	m_chipCount.store(newCount);
#ifdef IS_CPP_SERVER
	WriteAssetToDatabase();
#endif
}

int PlayerInfo::GetID() const
{
	auto rLock = m_lock.OnRead();
	return m_id;
}

int PlayerInfo::GetChip() const
{
	return m_chipCount.load();
}

std::string PlayerInfo::GetName() const
{
	auto rLock = m_lock.OnRead();
	return m_name;
}

Language PlayerInfo::GetLanguage() const
{
	auto rLock = m_lock.OnRead();
	return m_language;
}

void PlayerInfo::WriteInfo(NetPack& dst)
{
	auto rLock = m_lock.OnRead();
	dst.WriteUInt32(m_id);
	dst.WriteString(m_name);
	dst.WriteUInt8(m_language);
	dst.WriteInt32(m_chipCount.load());
}

void PlayerInfo::ReadInfo(NetPack& src)
{
	auto wLock = m_lock.OnWrite();
	m_id = src.ReadUInt32();
	m_name = src.ReadString();
	m_language = (Language)src.ReadUInt8();
	m_chipCount.store(src.ReadInt32());
}

#ifdef IS_CPP_SERVER
PlayerInfo::PlayerInfo(mysqlx::abi2::r0::Row& rowData)
	: m_chipCount(0)
{
	auto wLock = m_lock.OnWrite();
	m_id = rowData.get(2).get<int>();
	m_name = rowData.get(0).get<std::string>();
	m_language = (Language)rowData.get(1).get<int>();
	m_chipCount.store(rowData.get(3).get<int>());
}

void PlayerInfo::WriteInfoToDatabase()
{
	PlayerUtils::WriteUserInfoChangeToDatabase(*this);
}

void PlayerInfo::WriteAssetToDatabase()
{
	PlayerUtils::WriteUserAssetChangeToDatabase(*this);
}
#else
PlayerInfo::PlayerInfo(mysqlx::abi2::r0::Row& rowData) {}
void PlayerInfo::WriteInfoToDatabase() {}
void PlayerInfo::WriteAssetToDatabase() {}
#endif