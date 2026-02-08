#pragma once

#include "AI/HoldemBotDecision.h"
#include "Game/HoldemTableSnapshot.h"

class IHoldemBotStrategy
{
public:
	virtual ~IHoldemBotStrategy() = default;
	virtual HoldemDecision Decide(const HoldemTableSnapshot& snapshot,
		const HoldemDecisionSeat& seat,
		const HoldemDecisionOptions& options) = 0;
};
