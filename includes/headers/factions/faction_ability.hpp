#pragma once

#include <string>
#include <vector>

// Forward declarations
class Player;
struct territory;
struct treacheryCard;
struct PhaseContext;

/**
 * FactionAbility: base class for all faction-specific rule overrides.
 * Default implementations encode the standard rules (no faction advantage).
 * Concrete subclasses override only what their faction changes.
 *
 * Hook naming convention:
 *   get*()   → pure query, returns a value the phase uses
 *   on*()    → notification + optional mutation, phase continues after
 *   can*()   → permission check, returns bool
 */
class FactionAbility {
public:
	virtual ~FactionAbility() = default;

	// --- Identity ---
	virtual std::string getFactionName() const = 0;

	// --- Initialization ---
	virtual void setupAtStart(Player* player);
	virtual void placeStartingForces(PhaseContext& ctx);

	// --- Revival Phase hooks ---
	// How many forces this faction revives for free each turn
	virtual int getFreeRevivalsPerTurn() const;
	// Can this faction buy additional revivals beyond the free ones?
	virtual bool canBuyAdditionalRevivals() const;

	// --- Shipment Phase hooks ---
	// Cost in spice to ship N units to a given territory (default: 1 city, 2 other)
	virtual int getShipmentCost(const territory* terr, int unitCount) const;
	// Get valid territories for this faction to deploy to (Fremen: within 2 of Great Flat)
	// Returns empty vector to mean unrestricted (all except Polar Sink)
	virtual std::vector<std::string> getValidDeploymentTerritories(PhaseContext& ctx) const;
	// Can this faction ship from one on-planet territory to another? (Guild only)
	virtual bool canCrossShip() const;
	// Can this faction ship units back to reserves? (Guild only)
	virtual bool canShipToReserves() const;
	// Called when ANY other faction completes a shipment (BG piggyback hook)
	virtual void onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex);

	// --- Movement Phase hooks ---
	// Base movement range (before ornithopter bonus is applied)
	virtual int getBaseMovementRange() const;
	// Fremen: 2 territories base. Others: 1.
	// Note: ornithopter bonus (3 territories) is applied ON TOP by the phase if
	// the faction controls Arrakeen or Carthag, replacing base range with 3.

	// Can this faction take its shipment+move out of turn order? (Guild only)
	virtual bool canMoveOutOfTurnOrder() const;

	// --- Bidding Phase hooks ---
	// Maximum treachery cards this faction can hold (default 4, Harkonnen 8)
	virtual int getMaxTreacheryCards() const;
	// How many cards does this faction receive at game start? (default 1, Harkonnen 2)
	virtual int getStartingTreacheryCardCount() const;
	// Called when a card is dealt face-down for bidding, before any player bids.
	// Atreides overrides this to peek at the card.
	virtual void onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx);
	// Called when this faction wins a treachery card at auction.
	// Harkonnen overrides to draw a bonus card.
	virtual void onCardWonAtAuction(PhaseContext& ctx);
	// Called when another faction pays spice for a treachery card.
	// Emperor overrides to redirect payment to self instead of Spice Bank.
	virtual void onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount);

	// --- CHOAM Charity Phase hooks ---
	// Does this faction always receive charity regardless of spice count? (BG only)
	virtual bool alwaysReceivesCharity() const;

	// --- Battle Phase hooks ---
	// Fighting strength of a normal unit (default 1, Sardaukar/Fedaykin 2 vs non-Fremen)
	virtual int getNormalUnitStrength() const;
	// Does this faction need to spend spice to count units at full strength?
	// (Advanced game rule — default true, Fremen false)
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
