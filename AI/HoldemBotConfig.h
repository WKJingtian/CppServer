#pragma once

#include <atomic>
#include <string>

class HoldemBotConfig
{
public:
	static void Init(int idBase, int startingChips, std::string defaultName, int thinkTimeMs);
	static int NextId();
	static int GetStartingChips();
	static int GetThinkTimeMs();
	static const std::string& GetDefaultName();

private:
	static std::atomic<int> s_nextId;
	static int s_startingChips;
	static int s_thinkTimeMs;
	static std::string s_defaultName;
};
