#include "factions/spacing_guild_ability.hpp"

#include "phases/phase_context.hpp"
#include "player.hpp"
#include "map.hpp"
#include "logger/event_logger.hpp"
#include "events/event.hpp"
#include "settings.hpp"

std::string SpacingGuildAbility::getFactionName() const { return "Spacing Guild"; }

void SpacingGuildAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(5);
}

void SpacingGuildAbility::placeStartingForces(PhaseContext& ctx) {
	int guildIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Spacing Guild") {
			guildIndex = static_cast<int>(i);
			break;
		}
	}

	if (guildIndex < 0) return;

	Player* guild = ctx.players[guildIndex];
	int unitsToDeploy = std::min(5, guild->getUnitsReserve());
	if (unitsToDeploy <= 0) return;

	ctx.map.addUnitsToTerritory("Tuek's Sietch", guildIndex, unitsToDeploy, 0, -1);
	guild->deployUnits(unitsToDeploy);

	if (ctx.logger) {
		ctx.logger->logDebug("[Spacing Guild] Starts with " + std::to_string(unitsToDeploy) +
			" units in Tuek's Sietch and " + std::to_string(guild->getUnitsReserve()) + " in reserve");
	}
}

int SpacingGuildAbility::getFreeRevivalsPerTurn() const {
	return 1;
}

int SpacingGuildAbility::getShipmentCost(const territory* terr, int unitCount) const {
	int baseCost = FactionAbility::getShipmentCost(terr, unitCount);
	// Guild ships at half-rate, rounded up.
	return (baseCost + 1) / 2;
}

void SpacingGuildAbility::onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount,
	const std::string& destinationTerritory, bool fromOffPlanet) {
	(void)destinationTerritory;
	(void)fromOffPlanet;
	if (amount <= 0) return;

	int guildIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Spacing Guild") {
			guildIndex = static_cast<int>(i);
			break;
		}
	}

	if (guildIndex < 0 || guildIndex == shippingFactionIndex) {
		return;
	}

	ctx.players[guildIndex]->addSpice(amount);

	if (ctx.logger) {
		Event e(EventType::UNITS_SHIPPED,
			"[Spacing Guild] Collects " + std::to_string(amount) + " spice from " +
			ctx.players[shippingFactionIndex]->getFactionName() + " shipment",
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = "Spacing Guild";
		e.spiceValue = amount;
		ctx.logger->logEvent(e);
	}
}

bool SpacingGuildAbility::canCrossShip() const {
	return true;
}

bool SpacingGuildAbility::canShipToReserves() const {
	return true;
}

bool SpacingGuildAbility::canMoveOutOfTurnOrder() const {
	return true;
}

bool SpacingGuildAbility::checkSpecialVictory(PhaseContext& ctx) const {
	return ctx.turnNumber >= MAX_TURNS;
}
