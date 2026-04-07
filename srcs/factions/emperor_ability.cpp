#include "factions/emperor_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "map.hpp"

std::string EmperorAbility::getFactionName() const {
	return "Emperor";
}

void EmperorAbility::onOtherFactionPaidForCard(PhaseContext& ctx, int payingFactionIndex, int amount) {
	// Find Emperor player
	int emperorIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Emperor") {
			emperorIndex = i;
			break;
		}
	}
	
	if (emperorIndex < 0 || emperorIndex == payingFactionIndex) {
		return;  // Emperor not in game or Emperor is paying (shouldn't happen)
	}
	
	// Emperor receives the spice payment
	ctx.players[emperorIndex]->addSpice(amount);
	
	if (ctx.logger) {
		ctx.logger->logDebug(
			"Emperor receives " + std::to_string(amount) + " spice from " +
			ctx.players[payingFactionIndex]->getFactionName() + "'s card purchase"
		);
	}
}

void EmperorAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(10);  // Emperor start with 10 spice
}

void EmperorAbility::placeStartingForces(PhaseContext& ctx) {
	// Find Emperor player index
	int emperorIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Emperor") {
			emperorIndex = i;
			break;
		}
	}
	
	if (emperorIndex < 0) return;
	
	Player* emperor = ctx.players[emperorIndex];
	
	// Place 10 units on the map starting position
	ctx.map.addUnitsToTerritory("Arrakeen", emperorIndex, 10, 0);
	emperor->deployUnits(10);
	
	// Remaining 10 units in reserve
}
