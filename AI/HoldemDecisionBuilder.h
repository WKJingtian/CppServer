#pragma once

#include "AI/HoldemBotDecision.h"
#include "Game/HoldemTableSnapshot.h"

class HoldemDecisionBuilder
{
public:
	static bool Build(const HoldemTableSnapshot& snapshot,
		HoldemDecisionSeat& outSeat,
		HoldemDecisionOptions& outOptions);
};
