#pragma once

#include <vector>
#include <string>
#include <random>

enum class spiceCardType {
	LOCATION,
	WORM
};

struct spiceCard {
	spiceCardType type;
	std::string territoryName;
	int spiceAmount;
};

class GameMap; // Forward declaration

class SpiceDeck {
	private:
		std::vector<spiceCard> deck;
		size_t deckIndex;
		std::vector<spiceCard> discardPileA;
		std::vector<spiceCard> discardPileB;
		bool useExtendedSpiceBlow;
		std::mt19937& rng;

		void reshuffle();

	public:
		SpiceDeck(std::mt19937& rng_);

		void initialize(GameMap& map);
		spiceCard drawCard();
		spiceCard peekTopCard();
		std::vector<spiceCard> peekNextCards(int count);
		void discardCard(const spiceCard& card, int discardPileIndex);
		
		// Getters
		int remainingCards() const;
		int getTotalCards() const;
		bool isUsingExtendedSpiceBlow() const;
		void setUseExtendedSpiceBlow(bool extended);
		
		// Access to discard piles (for phases that need to check them)
		const std::vector<spiceCard>& getDiscardPileA() const;
		const std::vector<spiceCard>& getDiscardPileB() const;
		std::vector<spiceCard>& getDiscardPileAMutable();
		std::vector<spiceCard>& getDiscardPileBMutable();
};
