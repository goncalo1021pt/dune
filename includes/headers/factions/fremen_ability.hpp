#pragma once
#include "factions/faction_ability.hpp"

class FremenAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Revival Phase hooks ---
	int getFreeRevivalsPerTurn() const override;
	bool canBuyAdditionalRevivals() const override;

	// --- Movement Phase hooks ---
	int getBaseMovementRange() const override;

	// --- Shipment Phase hooks ---
	int getShipmentCost(const territory* terr, int unitCount) const override;
	std::vector<std::string> getValidDeploymentTerritories(PhaseContext& ctx) const override;

	// --- Battle Phase hooks ---
	bool requiresSpiceForFullUnitStrength() const override;

	// --- Storm/Spice Blow hooks ---
	bool survivesWorm() const override;
	bool hasReducedStormLosses() const override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;
};
