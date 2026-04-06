#include <cards/spice_deck.hpp>
#include <map.hpp>
#include <algorithm>
#include <iostream>

SpiceDeck::SpiceDeck(std::mt19937& rng_)
	: deck(), deckIndex(0), discardPileA(), discardPileB(),
	  useExtendedSpiceBlow(false), rng(rng_) {
}

void SpiceDeck::initialize(GameMap& map) {
	deck.clear();
	deckIndex = 0;
	discardPileA.clear();
	discardPileB.clear();

	// Create location cards from terrain with spice
	for (const auto& terr : map.getTerritories()) {
		territory* mutableTerritory = map.getTerritory(terr.name);
		if (mutableTerritory == nullptr) {
			continue;
		}

		if (terr.terrain == terrainType::desert && terr.spiceAmount > 0) {
			spiceCard locationCard;
			locationCard.type = spiceCardType::LOCATION;
			locationCard.territoryName = terr.name;
			locationCard.spiceAmount = terr.spiceAmount;
			deck.push_back(locationCard);
		}

		mutableTerritory->spiceAmount = 0;
	}

	// Add worm cards
	for (int i = 0; i < 6; ++i) {
		spiceCard wormCard;
		wormCard.type = spiceCardType::WORM;
		wormCard.territoryName = "";
		wormCard.spiceAmount = 0;
		deck.push_back(wormCard);
	}

	std::shuffle(deck.begin(), deck.end(), rng);
}

spiceCard SpiceDeck::drawCard() {
	if (deckIndex >= deck.size()) {
		reshuffle();
	}

	if (deck.empty()) {
		// Fallback: return a worm card if no cards available
		spiceCard fallback;
		fallback.type = spiceCardType::WORM;
		fallback.territoryName = "";
		fallback.spiceAmount = 0;
		return fallback;
	}

	spiceCard card = deck[deckIndex];
	deckIndex++;
	return card;
}

void SpiceDeck::discardCard(const spiceCard& card, int discardPileIndex) {
	if (useExtendedSpiceBlow && discardPileIndex == 1) {
		discardPileB.push_back(card);
	} else {
		discardPileA.push_back(card);
	}
}

void SpiceDeck::reshuffle() {
	if (discardPileA.empty() && discardPileB.empty()) {
		return;
	}

	deck.clear();
	deck.insert(deck.end(), discardPileA.begin(), discardPileA.end());
	deck.insert(deck.end(), discardPileB.begin(), discardPileB.end());
	discardPileA.clear();
	discardPileB.clear();
	std::shuffle(deck.begin(), deck.end(), rng);
	deckIndex = 0;
	std::cout << "    Spice deck reshuffled from discard piles" << std::endl;
}

int SpiceDeck::remainingCards() const {
	return static_cast<int>(deck.size() - deckIndex);
}

int SpiceDeck::getTotalCards() const {
	return static_cast<int>(deck.size() + discardPileA.size() + discardPileB.size());
}

bool SpiceDeck::isUsingExtendedSpiceBlow() const {
	return useExtendedSpiceBlow;
}

void SpiceDeck::setUseExtendedSpiceBlow(bool extended) {
	useExtendedSpiceBlow = extended;
}

const std::vector<spiceCard>& SpiceDeck::getDiscardPileA() const {
	return discardPileA;
}

const std::vector<spiceCard>& SpiceDeck::getDiscardPileB() const {
	return discardPileB;
}

std::vector<spiceCard>& SpiceDeck::getDiscardPileAMutable() {
	return discardPileA;
}

std::vector<spiceCard>& SpiceDeck::getDiscardPileBMutable() {
	return discardPileB;
}
