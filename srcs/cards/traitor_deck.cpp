#include "cards/traitor_deck.hpp"
#include <algorithm>

TraitorDeck::TraitorDeck(std::mt19937& rng_)
	: deck(), deckIndex(0), rng(rng_) {
}

void TraitorDeck::initialize() {
	deck.clear();
	deckIndex = 0;

	const traitorCard baseCards[] = {
		// Atreides
		{1, "Thufir Hawat", "Atreides", "Mentat of House Atreides"},
		{2, "Lady Jessica", "Atreides", "Bene Gesserit trainer"},
		{3, "Gurney Halleck", "Atreides", "Master of arms"},
		{4, "Duncan Idaho", "Atreides", "Swordmaster"},
		{5, "Dr. Wellington Yueh", "Atreides", "Atreides doctor"},
		
		// Bene Gesserit
		{6, "Alia", "Bene Gesserit", "Abomination"},
		{7, "Margot Lady Fenring", "Bene Gesserit", "Wife of Fenring"},
		{8, "Mother Ramallo", "Bene Gesserit", "Fremen Elder"},
		{9, "Princess Irulan", "Bene Gesserit", "Imperial historian"},
		{10, "Wanna Yueh", "Bene Gesserit", "Bene Gesserit wife"},
		
		// Emperor
		{11, "Hasimir Fenring", "Emperor", "Imperial Spy"},
		{12, "Captain Aramsham", "Emperor", "Imperial Captain"},
		{13, "Caid", "Emperor", "Imperial Caid"},
		{14, "Burseg", "Emperor", "Imperial Burseg"},
		{15, "Bashar", "Emperor", "Imperial Bashar"},
		
		// Fremen
		{16, "Stilgar", "Fremen", "Fremen leader"},
		{17, "Chani", "Fremen", "Fremen warrior"},
		{18, "Otheym", "Fremen", "Fremen scout"},
		{19, "Shadout Mapes", "Fremen", "Keeper of the well"},
		{20, "Jamis", "Fremen", "Fremen fighter"},
		
		// Spacing Guild
		{21, "Staban Tuek", "Spacing Guild", "Tuek representative"},
		{22, "Master Bewt", "Spacing Guild", "Guild master"},
		{23, "Esmar Tuek", "Spacing Guild", "Tuek merchant"},
		{24, "Soo-Soo Sook", "Spacing Guild", "Navigator"},
		{25, "Guild Rep", "Spacing Guild", "Guild representative"},
		
		// Harkonnen
		{26, "Feyd-Rautha", "Harkonnen", "Harkonnen heir"},
		{27, "Beast Rabban", "Harkonnen", "Harkonnen brute"},
		{28, "Piter de Vries", "Harkonnen", "Mentat of House Harkonnen"},
		{29, "Captain Iakin Nefud", "Harkonnen", "Harkonnen captain"},
		{30, "Umman Kudu", "Harkonnen", "Harkonnen soldier"},
	};

	for (const auto& card : baseCards) {
		deck.push_back(card);
	}

	// Shuffle the deck
	std::shuffle(deck.begin(), deck.end(), rng);
}

void TraitorDeck::reshuffle() {
	std::shuffle(deck.begin(), deck.end(), rng);
	deckIndex = 0;
}

traitorCard TraitorDeck::drawCard() {
	if (deckIndex >= deck.size()) {
		reshuffle();
	}

	traitorCard card = deck[deckIndex];
	deckIndex++;
	return card;
}

int TraitorDeck::remainingCards() const {
	if (deck.empty()) {
		return 0;
	}
	return deck.size() - deckIndex;
}

int TraitorDeck::getTotalCards() const {
	return deck.size();
}
