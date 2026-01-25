#pragma once
#include "Card.h"
#include <vector>
#include <random>

class Deck
{
public:
	Deck() { Reset52(); }

	void Reset52()
	{
		_cards.clear();
		_cards.reserve(52);
		for (uint8_t suit = 0; suit < Card::SUIT_COUNT; ++suit)
			for (uint8_t rank = Card::RANK_MIN; rank <= Card::RANK_MAX; ++rank)
				_cards.emplace_back(rank, suit);
	}

	void Shuffle(std::mt19937& rng)
	{
		std::shuffle(_cards.begin(), _cards.end(), rng);
	}

	Card Draw()
	{
		if (_cards.empty())
			return Card{};
		Card c = _cards.back();
		_cards.pop_back();
		return c;
	}

	size_t Size() const { return _cards.size(); }
	bool Empty() const { return _cards.empty(); }

private:
	std::vector<Card> _cards{};
};
