#pragma once

#include <cstdint>
#include <vector>

enum class HoldemDecisionAction : uint8_t
{
	CheckCall = 0,
	BetRaise = 1,
	Fold = 2
};

enum class HoldemBetSize : uint8_t
{
	OneThirdPot = 0,
	HalfPot = 1,
	TwoThirdPot = 2,
	Pot = 3,
	OneHalfPot = 4,
	TwoPot = 5,
	ThreePot = 6,
	AllIn = 7
};

struct HoldemBetOption
{
	HoldemBetSize size = HoldemBetSize::Pot;
	int raiseTo = 0;
};

struct HoldemDecision
{
	HoldemDecisionAction action = HoldemDecisionAction::CheckCall;
	HoldemBetSize betSize = HoldemBetSize::Pot;
	int raiseTo = 0;
};

struct HoldemDecisionSeat
{
	int playerId = -1;
	int seatIndex = -1;
	int seatListIndex = -1;
};

struct HoldemDecisionOptions
{
	bool canCheck = false;
	bool canCall = false;
	bool canRaise = false;
	bool canFold = false;
	int toCall = 0;
	int minRaiseTo = 0;
	int maxRaiseTo = 0;
	int pot = 0;
	std::vector<HoldemBetOption> betOptions{};
};
