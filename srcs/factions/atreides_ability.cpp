#include "factions/atreides_ability.hpp"

#include "player.hpp"
#include "phases/phase_context.hpp"
#include "logger/event_logger.hpp"
#include "cards/spice_deck.hpp"
#include "map.hpp"
#include "cards/treachery_deck.hpp"

std::string AtreidesAbility::getFactionName() const { return "Atreides"; }

void AtreidesAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(10);
}

void AtreidesAbility::placeStartingForces(PhaseContext& ctx) {
	int atreidesIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Atreides") {
			atreidesIndex = static_cast<int>(i);
			break;
		}
	}

	if (atreidesIndex < 0) return;

	Player* atreides = ctx.players[atreidesIndex];
	int unitsToDeploy = std::min(10, atreides->getUnitsReserve());
	if (unitsToDeploy <= 0) return;

	ctx.map.addUnitsToTerritory("Arrakeen", atreidesIndex, unitsToDeploy, 0, -1);
	atreides->deployUnits(unitsToDeploy);

	if (ctx.logger) {
		ctx.logger->logDebug("[Atreides] Starts with 10 units in Arrakeen and " +
			std::to_string(atreides->getUnitsReserve()) + " in reserve");
	}
}

int AtreidesAbility::getFreeRevivalsPerTurn() const {
	return 2;
}

void AtreidesAbility::onCardDealtForBidding(const treacheryCard& card, PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("[Atreides Prescience] Upcoming Treachery card: " + card.name);
	}
}
