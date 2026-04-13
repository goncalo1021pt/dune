#pragma once

#include <string>
#include <vector>

// Forward declarations
class Player;
struct territory;
struct treacheryCard;
struct PhaseContext;

class FactionAbility {
public:
	virtual ~FactionAbility() = default;

	// --- Identity ---
	virtual std::string getFactionName() const = 0;

	// --- Initialization ---
	virtual void setupAtStart(Player* player);
	virtual void placeStartingForces(PhaseContext& ctx);

	// --- Revival Phase hooks ---
	virtual int getFreeRevivalsPerTurn() const;
	virtual bool canBuyAdditionalRevivals() const;

	// --- Shipment Phase hooks ---
	virtual int getShipmentCost(const territory* terr, int unitCount) const;
	virtual std::vector<std::string> getValidDeploymentTerritories(PhaseContext& ctx) const;
	virtual bool canCrossShip() const;
	virtual bool canShipToReserves() const;
	virtual void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount);
	virtual void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount,
		const std::string& destinationTerritory, bool fromOffPlanet);

	// --- Movement Phase hooks ---
	virtual int getBaseMovementRange() const;
	virtual bool canMoveOutOfTurnOrder() const;

	// --- Bidding Phase hooks ---
	virtual int getMaxTreacheryCards() const;
	virtual int getStartingTreacheryCardCount() const;
	virtual void onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx);
	virtual void onCardWonAtAuction(PhaseContext& ctx);
	virtual void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount);

	// --- CHOAM Charity Phase hooks ---
	virtual bool alwaysReceivesCharity() const;
	virtual bool canUseWorthlessAsKarama() const;

	// --- Battle Phase hooks ---
	virtual int getNormalUnitStrength() const;
	virtual bool requiresSpiceForFullUnitStrength() const;
	// Called before battle plans are revealed. BG Voice intercepts here.
	virtual void onBeforeBattlePlanReveal(PhaseContext& ctx, int opponentIndex);
	// Called when this faction's opponent reveals their battle plan.
	// Atreides prescience intercepts here.
	virtual void onOpponentBattlePlanRevealed(PhaseContext& ctx, int opponentIndex);
	// Called after this faction wins a battle (Harkonnen captures leaders here)
	virtual void onBattleWon(PhaseContext& ctx, int opponentIndex);

	// --- Traitor hooks ---
	// Does this faction keep ALL traitor cards drawn? (Harkonnen only)
	virtual bool keepsAllTraitorCards() const;

	// --- Storm/Spice Blow hooks ---
	// Does this faction's forces survive a sandworm? (Fremen only)
	virtual bool survivesWorm() const;
	// Does this faction lose only half forces in a storm? (Fremen only)
	virtual bool hasReducedStormLosses() const;
	// Can this faction's units ride a sandworm to relocate? (Fremen only)
	virtual bool canRideWorm() const;
	// Called when a worm hits a territory containing this faction's units.
	// Allows the faction to move units before they're destroyed.
	// Return true if the faction moved units (Fremen), false otherwise.
	virtual bool onWormHitsTerritory(PhaseContext& ctx, const std::string& territoryName);

	// --- Victory condition hooks ---
	// Check faction-specific special victory condition.
	// Return true if this faction wins by special condition this turn.
	virtual bool checkSpecialVictory(PhaseContext& ctx) const;

	// --- Alliance hooks ---
	// Spice this faction provides per ally per revival phase (Emperor: pays for 3 extra)
	virtual int getAllyRevivalSupport() const;
};
