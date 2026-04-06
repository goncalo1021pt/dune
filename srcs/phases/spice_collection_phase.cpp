#include "phases/spice_collection_phase.hpp"
#include "game.hpp"
#include "player.hpp"
#include "map.hpp"
#include <iostream>

void SpiceCollectionPhase::execute(PhaseContext& ctx) {
	std::cout << "  SPICE_COLLECTION Phase" << std::endl;

	// Step 1: Add spice from three special cities
	addCitySpiceProduction(ctx);

	// Step 2: Collect spice from territories
	collectSpiceFromTerritories(ctx);
}

void SpiceCollectionPhase::addCitySpiceProduction(PhaseContext& ctx) {
	std::cout << "    City Tax Collection:" << std::endl;
	
	// Cities don't hold spice; instead they provide tax (spice income) to their controller
	const std::string cities[] = {"Arrakeen", "Carthag", "Tuek's Sietch"};
	const int cityTaxAmount[] = {2, 2, 1}; // Arrakeen: 2, Carthag: 2, Tuek's Sietch: 1
	
	for (int i = 0; i < 3; ++i) {
		territory* city = ctx.map.getTerritory(cities[i]);
		if (city != nullptr && !city->unitsPresent.empty()) {
			// City provides tax to all factions that have units there
			for (const auto& unitStack : city->unitsPresent) {
				int tax = cityTaxAmount[i];
				ctx.players[unitStack.factionOwner]->addSpice(tax);
				std::cout << "      " << ctx.players[unitStack.factionOwner]->getFactionName()
				          << " collects " << tax << " spice tax from " << cities[i] << std::endl;
			}
		}
	}
}

void SpiceCollectionPhase::collectSpiceFromTerritories(PhaseContext& ctx) {
	std::cout << "    Unit Spice Collection:" << std::endl;
	
	for (const auto& territory : ctx.map.getTerritories()) {
		if (territory.spiceAmount <= 0 || territory.unitsPresent.empty()) {
			continue;
		}

		for (const auto& unitStack : territory.unitsPresent) {
			int totalUnits = unitStack.normal_units + unitStack.elite_units;
			if (totalUnits <= 0) {
				continue;
			}

			// Each unit collects 2 spice
			int spiceCollected = std::min(totalUnits * 2, territory.spiceAmount);
			
			// Add to player
			ctx.players[unitStack.factionOwner]->addSpice(spiceCollected);
			
			// Remove from territory
			ctx.map.getTerritory(territory.name)->spiceAmount -= spiceCollected;

			std::cout << "      " << ctx.players[unitStack.factionOwner]->getFactionName()
			          << " collects " << spiceCollected << " spice from " << territory.name
			          << " (" << ctx.players[unitStack.factionOwner]->getSpice() << " total)" << std::endl;
		}
	}
}
