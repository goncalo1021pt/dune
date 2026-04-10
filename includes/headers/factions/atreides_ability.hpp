#pragma once
#include "factions/faction_ability.hpp"

class AtreidesAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;

	// --- Revival Phase hooks ---
	int getFreeRevivalsPerTurn() const override;

	// --- Bidding / Movement prescience hooks ---
	void onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx) override;
};
