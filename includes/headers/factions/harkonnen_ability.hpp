#pragma once
#include "factions/faction_ability.hpp"

class HarkonnenAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Bidding Phase hooks ---
	int getMaxTreacheryCards() const override;
	int getStartingTreacheryCardCount() const override;
	void onCardWonAtAuction(PhaseContext& ctx) override;

	// --- Traitor hooks ---
	bool keepsAllTraitorCards() const override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;
};
