#include "factions/faction_ability.hpp"
#include "map.hpp"
#include "player.hpp"

void FactionAbility::setupAtStart(Player* player) { (void)player; }
void FactionAbility::placeStartingForces(PhaseContext& ctx) { (void)ctx; }

int FactionAbility::getFreeRevivalsPerTurn() const { return 1; }
bool FactionAbility::canBuyAdditionalRevivals() const { return true; }

int FactionAbility::getShipmentCost(const territory* terr, int unitCount) const {
	if (terr == nullptr || unitCount <= 0) return 0;
	if (terr->terrain == terrainType::city) return unitCount * 1;
	return unitCount * 2;
}

std::vector<std::string> FactionAbility::getValidDeploymentTerritories(PhaseContext& /* ctx */) const {
	return {};
}

bool FactionAbility::canCrossShip() const { return false; }
bool FactionAbility::canShipToReserves() const { return false; }
void FactionAbility::onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex) { (void)ctx; (void)shippingFactionIndex; }

int FactionAbility::getBaseMovementRange() const { return 1; }
bool FactionAbility::canMoveOutOfTurnOrder() const { return false; }

int FactionAbility::getMaxTreacheryCards() const { return 4; }
int FactionAbility::getStartingTreacheryCardCount() const { return 1; }
void FactionAbility::onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx) { (void)card; (void)ctx; }
void FactionAbility::onCardWonAtAuction(PhaseContext& ctx) { (void)ctx; }
void FactionAbility::onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) { (void)ctx; (void)payingFactionIndex; (void)amount; }

bool FactionAbility::alwaysReceivesCharity() const { return false; }

int FactionAbility::getNormalUnitStrength() const { return 1; }
bool FactionAbility::requiresSpiceForFullUnitStrength() const { return true; }
void FactionAbility::onBeforeBattlePlanReveal(PhaseContext& ctx, int opponentIndex) { (void)ctx; (void)opponentIndex; }
void FactionAbility::onOpponentBattlePlanRevealed(PhaseContext& ctx, int opponentIndex) { (void)ctx; (void)opponentIndex; }

bool FactionAbility::keepsAllTraitorCards() const { return false; }

bool FactionAbility::survivesWorm() const { return false; }
bool FactionAbility::hasReducedStormLosses() const { return false; }
bool FactionAbility::canRideWorm() const { return false; }
bool FactionAbility::onWormHitsTerritory(PhaseContext& ctx, const std::string& territoryName) { (void)ctx; (void)territoryName; return false; }

bool FactionAbility::checkSpecialVictory(PhaseContext& ctx) const { (void)ctx; return false; }

int FactionAbility::getAllyRevivalSupport() const { return 0; }

