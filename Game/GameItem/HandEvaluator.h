#pragma once
#include "Card.h"
#include <array>
#include <vector>
#include <algorithm>

#undef min
#undef max

class HandEvaluator
{
public:
	enum class HandRank : int
	{
		HighCard = 0,
		OnePair = 1,
		TwoPair = 2,
		ThreeOfAKind = 3,
		Straight = 4,
		Flush = 5,
		FullHouse = 6,
		FourOfAKind = 7,
		StraightFlush = 8,
		RoyalFlush = 9
	};

	// Returns a score where higher is better
	// Format: (HandRank * 10000000) + tiebreakers
	static int Evaluate(const std::array<Card, 7>& cards)
	{
		std::array<Card, 7> sorted = cards;
		std::sort(sorted.begin(), sorted.end(), [](const Card& a, const Card& b) {
			return a.Rank() > b.Rank();
		});

		// Count ranks and suits
		std::array<int, 15> rankCount{};
		std::array<int, 4> suitCount{};
		for (const auto& c : sorted)
		{
			if (!c.IsValid()) continue;
			rankCount[c.Rank()]++;
			suitCount[c.Suit()]++;
		}

		// Check flush
		int flushSuit = -1;
		for (int s = 0; s < 4; ++s)
			if (suitCount[s] >= 5)
				flushSuit = s;

		// Collect flush cards if flush exists
		std::vector<int> flushRanks;
		if (flushSuit >= 0)
		{
			for (const auto& c : sorted)
				if (c.Suit() == flushSuit)
					flushRanks.push_back(c.Rank());
		}

		// Check straight (including wheel A-2-3-4-5)
		auto findStraight = [](const std::array<int, 15>& rc) -> int {
			// Check A-high to 6-high straights
			for (int high = 14; high >= 6; --high)
			{
				bool found = true;
				for (int i = 0; i < 5; ++i)
				{
					if (rc[high - i] == 0)
					{
						found = false;
						break;
					}
				}
				if (found) return high;
			}
			// Check wheel (A-2-3-4-5)
			if (rc[14] && rc[2] && rc[3] && rc[4] && rc[5])
				return 14;
			return 0;
		};

		auto findStraightInRanks = [](const std::vector<int>& ranks) -> int {
			if (ranks.size() < 5) return 0;
			std::array<int, 15> rc{};
			for (int r : ranks) rc[r]++;
			for (int high = 14; high >= 6; --high)
			{
				bool found = true;
				for (int i = 0; i < 5; ++i)
					if (rc[high - i] == 0) { found = false; break; }
				if (found) return high;
			}
			if (rc[14] && rc[2] && rc[3] && rc[4] && rc[5])
				return 14;
			return 0;
		};

		int straightHigh = findStraight(rankCount);
		int straightFlushHigh = flushSuit >= 0 ? findStraightInRanks(flushRanks) : 0;

		// Straight flush / Royal flush
		if (straightFlushHigh > 0)
		{
			if (straightFlushHigh == 14)
				return Score(HandRank::RoyalFlush, { 14 });
			return Score(HandRank::StraightFlush, { straightFlushHigh });
		}

		// Four of a kind
		for (int r = 14; r >= 2; --r)
		{
			if (rankCount[r] == 4)
			{
				int kicker = 0;
				for (int k = 14; k >= 2; --k)
					if (k != r && rankCount[k] > 0) { kicker = k; break; }
				return Score(HandRank::FourOfAKind, { r, kicker });
			}
		}

		// Full house
		int tripRank = 0, pairRank = 0;
		for (int r = 14; r >= 2; --r)
		{
			if (rankCount[r] >= 3 && tripRank == 0)
				tripRank = r;
			else if (rankCount[r] >= 2 && pairRank == 0)
				pairRank = r;
		}
		if (tripRank > 0 && pairRank > 0)
			return Score(HandRank::FullHouse, { tripRank, pairRank });

		// Flush
		if (flushSuit >= 0)
		{
			std::vector<int> kickers(flushRanks.begin(), flushRanks.begin() + std::min((size_t)5, flushRanks.size()));
			return Score(HandRank::Flush, kickers);
		}

		// Straight
		if (straightHigh > 0)
			return Score(HandRank::Straight, { straightHigh });

		// Three of a kind
		if (tripRank > 0)
		{
			std::vector<int> kickers;
			for (int r = 14; r >= 2 && kickers.size() < 2; --r)
				if (r != tripRank && rankCount[r] > 0)
					kickers.push_back(r);
			std::vector<int> vals = { tripRank };
			vals.insert(vals.end(), kickers.begin(), kickers.end());
			return Score(HandRank::ThreeOfAKind, vals);
		}

		// Two pair / One pair
		std::vector<int> pairs;
		for (int r = 14; r >= 2; --r)
			if (rankCount[r] >= 2)
				pairs.push_back(r);

		if (pairs.size() >= 2)
		{
			std::vector<int> kickers;
			for (int r = 14; r >= 2 && kickers.size() < 1; --r)
				if (r != pairs[0] && r != pairs[1] && rankCount[r] > 0)
					kickers.push_back(r);
			return Score(HandRank::TwoPair, { pairs[0], pairs[1], kickers.empty() ? 0 : kickers[0] });
		}

		if (pairs.size() == 1)
		{
			std::vector<int> kickers;
			for (int r = 14; r >= 2 && kickers.size() < 3; --r)
				if (r != pairs[0] && rankCount[r] > 0)
					kickers.push_back(r);
			std::vector<int> vals = { pairs[0] };
			vals.insert(vals.end(), kickers.begin(), kickers.end());
			return Score(HandRank::OnePair, vals);
		}

		// High card
		std::vector<int> kickers;
		for (int r = 14; r >= 2 && kickers.size() < 5; --r)
			if (rankCount[r] > 0)
				kickers.push_back(r);
		return Score(HandRank::HighCard, kickers);
	}
	
	/*
		HighCard = 0,
		OnePair = 1,
		TwoPair = 2,
		ThreeOfAKind = 3,
		Straight = 4,
		Flush = 5,
		FullHouse = 6,
		FourOfAKind = 7,
		StraightFlush = 8,
		RoyalFlush = 9
	*/
	static std::string ParseScore(int score)
	{
		int type = score / 10000000;
		switch (type)
		{
		case 0:
			return "HighCard";
		case 1:
			return "OnePair";
		case 2:
			return "TwoPair";
		case 3:
			return "ThreeOfAKind";
		case 4:
			return "Straight";
		case 5:
			return "Flush";
		case 6:
			return "FullHouse";
		case 7:
			return "FourOfAKind";
		case 8:
			return "StraightFlush";
		case 9:
			return "RoyalFlush";
		}
	}

private:
	static int Score(HandRank rank, const std::vector<int>& kickers)
	{
		int score = static_cast<int>(rank) * 10000000;
		int mult = 100000;
		for (size_t i = 0; i < kickers.size() && i < 5; ++i)
		{
			score += kickers[i] * mult;
			mult /= 100;
		}
		return score;
	}
};
