#pragma once

#include <vector>
#include <string>
#include <random>

struct traitorCard {
	int id;
	std::string name;
	std::string faction;
	std::string description;
};

class TraitorDeck {
	private:
		std::vector<traitorCard> deck;
		size_t deckIndex;
		std::mt19937& rng;
		void reshuffle();

	public:
		TraitorDeck(std::mt19937& rng_);

		void initialize();
		traitorCard drawCard();
		int remainingCards() const;
		int getTotalCards() const;
};
