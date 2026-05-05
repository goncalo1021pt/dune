#pragma once

#include <vector>
#include <string>
#include <random>

enum class treacheryCardType {
	WEAPON_PROJECTILE,
	WEAPON_POISON,
	WEAPON_SPECIAL,
	DEFENSE_PROJECTILE,
	DEFENSE_POISON,
	SPECIAL_LEADER,
	SPECIAL_STORM,
	SPECIAL_MOVE,
	SPECIAL,
	WORTHLESS,
};


struct treacheryCard {
	int id; // Unique identifier for the card
	std::string name;
	treacheryCardType type;
	std::string description;
	// Future: cardType type, int cost, bool isTraitor, etc.
};

class TreacheryDeck {
	private:
		std::vector<treacheryCard> deck;
		std::vector<std::string> discardPile;
		size_t deckIndex;
		std::mt19937& rng;
		void reshuffle();

	public:
		TreacheryDeck(std::mt19937& rng_);

		void initialize();
		treacheryCard drawCard();
		int remainingCards() const;
		int getTotalCards() const;

		// Discard pile: cards consumed (e.g. Karama) land here by name.
		// Player::treacheryCards stores names only, so the discard mirrors
		// that — full treacheryCard structs are not preserved on discard.
		void discard(const std::string& cardName);
		int discardSize() const;
		const std::vector<std::string>& getDiscardPile() const;
};
