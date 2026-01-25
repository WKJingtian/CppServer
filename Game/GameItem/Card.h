#pragma once
#include <cstdint>
#include <string>

class NetPack;

class Card
{
public:
	static constexpr uint8_t RANK_MIN = 2;
	static constexpr uint8_t RANK_MAX = 14; // Ace high
	static constexpr uint8_t SUIT_COUNT = 4;

	Card() = default;
	Card(uint8_t rank, uint8_t suit) : _rank(rank), _suit(suit) {}

	uint8_t Rank() const { return _rank; }
	uint8_t Suit() const { return _suit; }
	bool IsValid() const { return _rank >= RANK_MIN && _rank <= RANK_MAX && _suit < SUIT_COUNT; }

	std::string ToString() const
	{
		static const char* ranks[] = { "","","2","3","4","5","6","7","8","9","T","J","Q","K","A" };
		static const char* suits[] = { "spade","heart","diamond","club" };
		if (!IsValid()) return "??";
		return std::string(ranks[_rank]) + suits[_suit];
	}

	bool operator==(const Card& other) const { return _rank == other._rank && _suit == other._suit; }
	bool operator<(const Card& other) const
	{
		if (_rank != other._rank) return _rank < other._rank;
		return _suit < other._suit;
	}

	void Write(NetPack& pack) const;
	void Read(NetPack& pack);

private:
	uint8_t _rank = 0;
	uint8_t _suit = 0;
};
