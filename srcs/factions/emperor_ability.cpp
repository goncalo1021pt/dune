#include "factions/emperor_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "map.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"

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
	
	// Emperor receives the spice payment (instead of going to bank)
	ctx.players[emperorIndex]->addSpice(amount);
	
	if (ctx.logger) {
		Event e(EventType::BID_PLACED,
			"[Emperor] Intercepts " + std::to_string(amount) + " spice from " + 
			ctx.players[payingFactionIndex]->getFactionName() + "'s bid",
			ctx.turnNumber, "BIDDING");
		e.playerFaction = "Emperor";
		e.spiceValue = amount;
		ctx.logger->logEvent(e);
	}
}

void EmperorAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(10);  // Emperor starts with 10 spice
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

	if (ctx.featureSettings.advancedFactionAbilities) {
		// Advanced faction rules: 15 normal + 5 Sardaukar.
		emperor->setUnitsReserve(15);
		emperor->setEliteUnitsReserve(5);
		if (ctx.logger) {
			ctx.logger->logDebug("[Emperor] Starts with 15 normal units + 5 Sardaukar elite soldiers in reserves (advanced factions)");
		}
	} else {
		// Basic mode: no special units.
		emperor->setUnitsReserve(20);
		emperor->setEliteUnitsReserve(0);
		if (ctx.logger) {
			ctx.logger->logDebug("[Emperor] Starts with 20 normal units in reserves (basic mode)");
		}
	}
}
