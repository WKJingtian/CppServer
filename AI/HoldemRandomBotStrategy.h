#pragma once

#include "AI/HoldemBotStrategy.h"
#include <random>

class HoldemRandomBotStrategy : public IHoldemBotStrategy
{
public:
	HoldemRandomBotStrategy();
	explicit HoldemRandomBotStrategy(uint32_t seed);

	HoldemDecision Decide(const HoldemTableSnapshot& snapshot,
		const HoldemDecisionSeat& seat,
		const HoldemDecisionOptions& options) override;

private:
	std::mt19937 _rng;
};
