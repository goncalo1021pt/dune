#pragma once
#include "factions/faction_ability.hpp"

class EmperorAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Bidding Phase hooks ---
	// Emperor receives all spice payments made during auctions instead of spice going to bank
	void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	
	// Emperor starts outside the planet with Sardukar (elite) soldiers
	// All 5 elite units are NOT deployed initially and remain in reserves
	void placeStartingForces(PhaseContext& ctx) override;
};
