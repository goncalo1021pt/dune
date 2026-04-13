#include "phases/spice_collection_phase.hpp"
#include "game.hpp"
#include "player.hpp"
#include "map.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"

void SpiceCollectionPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("SPICE_COLLECTION Phase");
	}

	auto view = ctx.getSpiceCollectionView();
	// Step 1: Add spice from three special cities
	addCitySpiceProduction(view, ctx);

	// Step 2: Collect spice from territories
	collectSpiceFromTerritories(view, ctx);
}

void SpiceCollectionPhase::addCitySpiceProduction(PhaseContext::SpiceCollectionView& view, PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("City Tax Collection");
	}
	
	// Cities don't hold spice; instead they provide tax (spice income) to their controller
	const std::string cities[] = {"Arrakeen", "Carthag", "Tuek's Sietch"};
	const int cityTaxAmount[] = {2, 2, 1}; // Arrakeen: 2, Carthag: 2, Tuek's Sietch: 1
	
	for (int i = 0; i < 3; ++i) {
		territory* city = view.map.getTerritory(cities[i]);
		if (city != nullptr && !city->unitsPresent.empty()) {
			// City provides tax to all factions that have units there
			for (const auto& unitStack : city->unitsPresent) {
				if (unitStack.advisor) {
					continue;
				}
				int tax = cityTaxAmount[i];
				view.players[unitStack.factionOwner]->addSpice(tax);
				
				if (ctx.logger) {
					Event e(EventType::SPICE_COLLECTED,
						"collects " + std::to_string(tax) + " spice tax from " + cities[i],
						ctx.turnNumber, "SPICE_COLLECTION");
					e.playerFaction = view.players[unitStack.factionOwner]->getFactionName();
					e.territory = cities[i];
					e.spiceValue = tax;
					ctx.logger->logEvent(e);
				}
			}
		}
	}
}

void SpiceCollectionPhase::collectSpiceFromTerritories(PhaseContext::SpiceCollectionView& view, PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("Unit Spice Collection");
	}
	
	for (const auto& territory : view.map.getTerritories()) {
		int territorySpiceRemaining = view.map.getSpiceInTerritory(territory.name);
		if (territorySpiceRemaining <= 0 || territory.unitsPresent.empty()) {
			continue;
		}

		for (const auto& unitStack : territory.unitsPresent) {
			if (unitStack.advisor) {
				continue;
			}
			int totalUnits = unitStack.normal_units + unitStack.elite_units;
			if (totalUnits <= 0) {
				continue;
			}
			if (territorySpiceRemaining <= 0) {
				break;
			}

			// Each unit collects 2 spice from the territory regardless of sector.
			int requestedSpice = totalUnits * 2;
			int availableBeforeCollection = territorySpiceRemaining;
			int spiceCollected = view.map.removeSpiceFromTerritory(territory.name,
				std::min(requestedSpice, availableBeforeCollection));
			territorySpiceRemaining -= spiceCollected;
			
			if (spiceCollected > 0) {
				// Add to player
				view.players[unitStack.factionOwner]->addSpice(spiceCollected);

				if (ctx.logger) {
					std::string unitDesc = "(" + std::to_string(unitStack.normal_units) + " normal + " 
						+ std::to_string(unitStack.elite_units) + " elite)";
					std::string fullMessage = unitDesc + " collects " + std::to_string(spiceCollected)
						+ " spice from " + territory.name
						+ (spiceCollected < requestedSpice
							? " [only " + std::to_string(availableBeforeCollection) + " available]"
							: "");
					Event e(EventType::SPICE_COLLECTED,
						fullMessage,
						ctx.turnNumber, "SPICE_COLLECTION");
					e.playerFaction = view.players[unitStack.factionOwner]->getFactionName();
					e.territory = territory.name;
					e.spiceValue = spiceCollected;
					ctx.logger->logEvent(e);
				}
			}
		}
	}
}
