#include "factions/fremen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "map.hpp"

std::string FremenAbility::getFactionName() const {
	return "Fremen";
}

int FremenAbility::getFreeRevivalsPerTurn() const {
	return 3;  // Fremen revive 3 forces per turn for free
}

bool FremenAbility::canBuyAdditionalRevivals() const {
	return false;  // Fremen cannot buy additional revivals
}

int FremenAbility::getBaseMovementRange() const {
	return 2;  // Fremen can move 2 territories base (instead of 1)
}

bool FremenAbility::requiresSpiceForFullUnitStrength() const {
	return false;  // Fremen units count at full strength without spice
}

bool FremenAbility::survivesWorm() const {
	return true;  // Fremen units survive sandworms
}

bool FremenAbility::hasReducedStormLosses() const {
	return true;  // Fremen take half losses in storms
}

void FremenAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(3);  // Fremen start with 3 spice
}

void FremenAbility::placeStartingForces(PhaseContext& ctx) {
	// Find Fremen player index
	int fremenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Fremen") {
			fremenIndex = i;
			break;
		}
	}
	
	if (fremenIndex < 0) return;
	
	Player* fremen = ctx.players[fremenIndex];
	
	// Place 5 units in each of 3 territories starting positions
	ctx.map.addUnitsToTerritory("Sietch Tabr", fremenIndex, 5, 0);
	fremen->deployUnits(5);
	
	ctx.map.addUnitsToTerritory("False Wall South", fremenIndex, 3, 0);
	fremen->deployUnits(3);
	
	ctx.map.addUnitsToTerritory("False Wall West", fremenIndex, 2, 0);
	fremen->deployUnits(2);
	
	// Remaining units in reserve (total 20 deployed initially)
	// Note: 10 - (5+3+2) = 0 remain, all 10 are placed
}
