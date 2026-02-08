#include "pch.h"
#include "HoldemBotConfig.h"

std::atomic<int> HoldemBotConfig::s_nextId{ 20000000 };
int HoldemBotConfig::s_startingChips = 0;
int HoldemBotConfig::s_thinkTimeMs = 0;
std::string HoldemBotConfig::s_defaultName = "Bot";

void HoldemBotConfig::Init(int idBase, int startingChips, std::string defaultName, int thinkTimeMs)
{
	if (idBase <= 0)
		idBase = 20000000;
	if (startingChips < 0)
		startingChips = 0;
	if (thinkTimeMs < 0)
		thinkTimeMs = 0;
	if (defaultName.empty())
		defaultName = "Bot";

	s_nextId.store(idBase);
	s_startingChips = startingChips;
	s_thinkTimeMs = thinkTimeMs;
	s_defaultName = std::move(defaultName);
}

int HoldemBotConfig::NextId()
{
	return s_nextId.fetch_add(1);
}

int HoldemBotConfig::GetStartingChips()
{
	return s_startingChips;
}

int HoldemBotConfig::GetThinkTimeMs()
{
	return s_thinkTimeMs;
}

const std::string& HoldemBotConfig::GetDefaultName()
{
	return s_defaultName;
}
