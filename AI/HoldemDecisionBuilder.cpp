#include "pch.h"
#include "HoldemDecisionBuilder.h"
#include <algorithm>

namespace
{
	int ComputeRaiseTo(int lastBet, int pot, int numerator, int denom)
	{
		if (denom <= 0)
			return lastBet;
		int add = (pot * numerator) / denom;
		return lastBet + add;
	}

	void AddUniqueOption(HoldemDecisionOptions& options, HoldemBetSize size, int raiseTo)
	{
		for (const auto& existing : options.betOptions)
		{
			if (existing.raiseTo == raiseTo)
				return;
		}
		HoldemBetOption option{};
		option.size = size;
		option.raiseTo = raiseTo;
		options.betOptions.push_back(option);
	}
}

bool HoldemDecisionBuilder::Build(const HoldemTableSnapshot& snapshot,
	HoldemDecisionSeat& outSeat,
	HoldemDecisionOptions& outOptions)
{
	outSeat = HoldemDecisionSeat{};
	outOptions = HoldemDecisionOptions{};

	int actingPlayerId = snapshot.actingPlayerId;
	if (actingPlayerId < 0)
		return false;

	const Seat* seatPtr = nullptr;
	int seatListIndex = -1;
	for (size_t i = 0; i < snapshot.seats.size(); ++i)
	{
		const auto& seatSnapshot = snapshot.seats[i];
		if (seatSnapshot.seat.playerId == actingPlayerId)
		{
			seatPtr = &seatSnapshot.seat;
			seatListIndex = static_cast<int>(i);
			break;
		}
	}

	if (!seatPtr)
		return false;

	const Seat& seat = *seatPtr;
	if (!seat.inHand || seat.folded || seat.allIn)
		return false;

	outSeat.playerId = actingPlayerId;
	outSeat.seatIndex = seat.seatIndex;
	outSeat.seatListIndex = seatListIndex;

	outOptions.pot = snapshot.totalPot;
	outOptions.toCall = std::max(0, snapshot.lastBet - seat.currentBet);
	outOptions.minRaiseTo = snapshot.lastBet + snapshot.bigBlind;
	outOptions.maxRaiseTo = seat.currentBet + seat.chips;

	outOptions.canFold = true;
	outOptions.canCheck = outOptions.toCall == 0;
	outOptions.canCall = outOptions.toCall > 0 && seat.chips > 0;

	if (snapshot.bigBlind <= 0)
	{
		outOptions.canRaise = false;
		return true;
	}

	if (outOptions.maxRaiseTo >= outOptions.minRaiseTo && seat.chips > outOptions.toCall)
		outOptions.canRaise = true;

	if (!outOptions.canRaise)
		return true;

	struct SizeSpec
	{
		HoldemBetSize size;
		int numerator;
		int denom;
	};

	static const SizeSpec sizeSpecs[] =
	{
		{ HoldemBetSize::OneThirdPot, 1, 3 },
		{ HoldemBetSize::HalfPot, 1, 2 },
		{ HoldemBetSize::TwoThirdPot, 2, 3 },
		{ HoldemBetSize::Pot, 1, 1 },
		{ HoldemBetSize::OneHalfPot, 3, 2 },
		{ HoldemBetSize::TwoPot, 2, 1 },
		{ HoldemBetSize::ThreePot, 3, 1 }
	};

	for (const auto& spec : sizeSpecs)
	{
		int raiseTo = ComputeRaiseTo(snapshot.lastBet, outOptions.pot, spec.numerator, spec.denom);
		if (raiseTo < outOptions.minRaiseTo)
			raiseTo = outOptions.minRaiseTo;
		if (raiseTo > outOptions.maxRaiseTo)
			continue;
		AddUniqueOption(outOptions, spec.size, raiseTo);
	}

	AddUniqueOption(outOptions, HoldemBetSize::AllIn, outOptions.maxRaiseTo);
	return true;
}
