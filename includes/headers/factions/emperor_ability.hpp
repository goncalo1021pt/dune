#pragma once
#include "factions/faction_ability.hpp"

class EmperorAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Bidding Phase hooks ---
	void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;
};
