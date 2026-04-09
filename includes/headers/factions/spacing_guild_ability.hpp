#pragma once
#include "factions/faction_ability.hpp"

class SpacingGuildAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;

	// --- Revival Phase hooks ---
	int getFreeRevivalsPerTurn() const override;

	// --- Shipment Phase hooks ---
	int getShipmentCost(const territory* terr, int unitCount) const override;
	void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount) override;
	bool canCrossShip() const override;
	bool canShipToReserves() const override;

	// --- Movement Phase hooks ---
	bool canMoveOutOfTurnOrder() const override;
};
