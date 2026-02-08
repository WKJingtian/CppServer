#pragma once

#include "AI/HoldemBotConfig.h"
#include "AI/HoldemBotStrategy.h"
#include "Player/PlayerInfo.h"
#include <memory>

class HoldemBot
{
public:
	explicit HoldemBot(std::unique_ptr<IHoldemBotStrategy> strategy);
	HoldemBot(std::unique_ptr<IHoldemBotStrategy> strategy, int id, std::string name, int startingChips);

	HoldemDecision Decide(const HoldemTableSnapshot& snapshot,
		const HoldemDecisionSeat& seat,
		const HoldemDecisionOptions& options);

	PlayerInfo& GetInfo() { return _info; }
	const PlayerInfo& GetInfo() const { return _info; }
	int GetID() const { return _info.GetID(); }
	std::string GetName() const { return _info.GetName(); }
	bool HasStrategy() const { return _strategy != nullptr; }

private:
	std::unique_ptr<IHoldemBotStrategy> _strategy;
	PlayerInfo _info{};
};
