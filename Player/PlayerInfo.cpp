#include "pch.h"
#include "PlayerInfo.h"

PlayerInfo::PlayerInfo(int id, std::string name, int language)
{
	m_id = id;
	m_name = name;
	m_language = (Language)language;
}
PlayerInfo::~PlayerInfo()
{

}
void PlayerInfo::SetName(std::string n)
{
	m_name = n;
}
void PlayerInfo::SetLanguage(Language l)
{
	m_language = l;
}
void PlayerInfo::WriteInfo(NetPack& dst)
{
	dst.WriteString(m_name);
	dst.WriteUInt8(m_language);
}