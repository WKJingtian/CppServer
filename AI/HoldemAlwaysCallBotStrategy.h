#pragma once

#include "AI/HoldemBotStrategy.h"

class HoldemAlwaysCallBotStrategy : public IHoldemBotStrategy
{
public:
	HoldemDecision Decide(const HoldemTableSnapshot& snapshot,
		const HoldemDecisionSeat& seat,
		const HoldemDecisionOptions& options) override;
};
