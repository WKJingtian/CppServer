#include "pch.h"
#include "HoldemAlwaysCallBotStrategy.h"

HoldemDecision HoldemAlwaysCallBotStrategy::Decide(const HoldemTableSnapshot& snapshot,
	const HoldemDecisionSeat& seat,
	const HoldemDecisionOptions& options)
{
	(void)snapshot;
	(void)seat;

	HoldemDecision decision{};
	if (options.canCheck || options.canCall)
	{
		decision.action = HoldemDecisionAction::CheckCall;
		return decision;
	}

	if (options.canFold)
	{
		decision.action = HoldemDecisionAction::Fold;
		return decision;
	}

	if (options.canRaise && !options.betOptions.empty())
	{
		decision.action = HoldemDecisionAction::BetRaise;
		decision.betSize = options.betOptions.front().size;
		decision.raiseTo = options.betOptions.front().raiseTo;
	}
	return decision;
}
