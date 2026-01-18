#pragma once
enum Language : byte
{
	English = 0,
	Chinese = 1,
	Italian = 2,
	Japanese = 3,
	Franch = 4,
	Spanish = 5,
};

class NetPack;
class Player;
class PlayerMgr;
class PlayerInfo
{
	int m_id;
	std::string m_name;
	Language m_language;

public:
	PlayerInfo(int id, std::string name, int language);
	~PlayerInfo();

	void SetName(std::string n);
	void SetLanguage(Language l);

	void WriteInfo(NetPack& dst);

	friend Player;
	friend PlayerMgr;
};