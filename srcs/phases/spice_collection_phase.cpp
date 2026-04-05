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
	std::cout << "    City Spice Production:" << std::endl;
	
	ctx.map.getTerritory("Arrakeen")->spiceAmount += 2;
	std::cout << "      Arrakeen produces 2 spice" << std::endl;
	
	ctx.map.getTerritory("Carthag")->spiceAmount += 2;
	std::cout << "      Carthag produces 2 spice" << std::endl;
	
	ctx.map.getTerritory("Tuek's Sietch")->spiceAmount += 1;
	std::cout << "      Tuek's Sietch produces 1 spice" << std::endl;
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
