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
		{1, "Paul Muad'Dib", "Atreides", "Leader of Atreides forces"},
		{2, "Thufir Hawat", "Atreides", "Mentat of House Atreides"},
		{3, "Lady Jessica", "Atreides", "Bene Gesserit trainer"},
		{4, "Gurney Halleck", "Atreides", "Master of arms"},
		{5, "Duncan Idaho", "Atreides", "Swordmaster"},
		{6, "Dr. Wellington Yueh", "Atreides", "Atreides doctor"},
		
		// Bene Gesserit
		{7, "Mother Mohiam", "Bene Gesserit", "Reverend Mother"},
		{8, "Alia", "Bene Gesserit", "Abomination"},
		{9, "Margot Lady Fenring", "Bene Gesserit", "Wife of Fenring"},
		{10, "Mother Ramallo", "Bene Gesserit", "Fremen Elder"},
		{11, "Princess Irulan", "Bene Gesserit", "Imperial historian"},
		{12, "Wanna Yueh", "Bene Gesserit", "Bene Gesserit wife"},
		
		// Emperor
		{13, "Emperor Shaddam IV", "Emperor", "Padishah Emperor"},
		{14, "Hasimir Fenring", "Emperor", "Imperial Spy"},
		{15, "Captain Aramsham", "Emperor", "Imperial Captain"},
		{16, "Caid", "Emperor", "Imperial Caid"},
		{17, "Burseg", "Emperor", "Imperial Burseg"},
		{18, "Bashar", "Emperor", "Imperial Bashar"},
		
		// Fremen
		{19, "Liet Kynes", "Fremen", "Environmental Master"},
		{20, "Stilgar", "Fremen", "Fremen leader"},
		{21, "Chani", "Fremen", "Fremen warrior"},
		{22, "Otheym", "Fremen", "Fremen scout"},
		{23, "Shadout Mapes", "Fremen", "Keeper of the well"},
		{24, "Jamis", "Fremen", "Fremen fighter"},
		
		// Spacing Guild
		{25, "Edric", "Spacing Guild", "Master of the Spice"},
		{26, "Staban Tuek", "Spacing Guild", "Tuek representative"},
		{27, "Master Bewt", "Spacing Guild", "Guild master"},
		{28, "Esmar Tuek", "Spacing Guild", "Tuek merchant"},
		{29, "Soo-Soo Sook", "Spacing Guild", "Navigator"},
		{30, "Guild Rep", "Spacing Guild", "Guild representative"},
		
		// Harkonnen
		{31, "Baron Harkonnen", "Harkonnen", "Baron Vladimir Harkonnen"},
		{32, "Feyd-Rautha", "Harkonnen", "Harkonnen heir"},
		{33, "Beast Rabban", "Harkonnen", "Harkonnen brute"},
		{34, "Piter de Vries", "Harkonnen", "Mentat of House Harkonnen"},
		{35, "Captain Iakin Nefud", "Harkonnen", "Harkonnen captain"},
		{36, "Umman Kudu", "Harkonnen", "Harkonnen soldier"},
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
