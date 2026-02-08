#include "pch.h"
#include "HoldemRandomBotStrategy.h"
#include <vector>

HoldemRandomBotStrategy::HoldemRandomBotStrategy()
	: _rng(std::random_device{}())
{
}

HoldemRandomBotStrategy::HoldemRandomBotStrategy(uint32_t seed)
	: _rng(seed)
{
}

HoldemDecision HoldemRandomBotStrategy::Decide(const HoldemTableSnapshot& snapshot,
	const HoldemDecisionSeat& seat,
	const HoldemDecisionOptions& options)
{
	(void)snapshot;
	(void)seat;

	HoldemDecision decision{};

	std::vector<HoldemDecisionAction> actions{};
	if (options.canCheck || options.canCall)
		actions.push_back(HoldemDecisionAction::CheckCall);
	if (options.canRaise && !options.betOptions.empty())
		actions.push_back(HoldemDecisionAction::BetRaise);
	if (options.canFold)
		actions.push_back(HoldemDecisionAction::Fold);

	if (actions.empty())
		return decision;

	std::uniform_int_distribution<size_t> actionDist(0, actions.size() - 1);
	decision.action = actions[actionDist(_rng)];

	if (decision.action != HoldemDecisionAction::BetRaise)
		return decision;

	if (options.betOptions.empty())
	{
		if (options.canCheck || options.canCall)
			decision.action = HoldemDecisionAction::CheckCall;
		else if (options.canFold)
			decision.action = HoldemDecisionAction::Fold;
		return decision;
	}

	std::uniform_int_distribution<size_t> betDist(0, options.betOptions.size() - 1);
	const auto& choice = options.betOptions[betDist(_rng)];
	decision.betSize = choice.size;
	decision.raiseTo = choice.raiseTo;
	return decision;
}
