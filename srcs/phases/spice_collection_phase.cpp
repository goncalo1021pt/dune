#include "phases/spice_collection_phase.hpp"
#include "game.hpp"
#include "player.hpp"
#include "map.hpp"
#include <iostream>

void SpiceCollectionPhase::execute(PhaseContext& ctx) {
	std::cout << "  SPICE_COLLECTION Phase" << std::endl;

	auto view = ctx.getSpiceCollectionView();
	// Step 1: Add spice from three special cities
	addCitySpiceProduction(view);

	// Step 2: Collect spice from territories
	collectSpiceFromTerritories(view);
}

void SpiceCollectionPhase::addCitySpiceProduction(PhaseContext::SpiceCollectionView& view) {
	std::cout << "    City Tax Collection:" << std::endl;
	
	// Cities don't hold spice; instead they provide tax (spice income) to their controller
	const std::string cities[] = {"Arrakeen", "Carthag", "Tuek's Sietch"};
	const int cityTaxAmount[] = {2, 2, 1}; // Arrakeen: 2, Carthag: 2, Tuek's Sietch: 1
	
	for (int i = 0; i < 3; ++i) {
		territory* city = view.map.getTerritory(cities[i]);
		if (city != nullptr && !city->unitsPresent.empty()) {
			// City provides tax to all factions that have units there
			for (const auto& unitStack : city->unitsPresent) {
				int tax = cityTaxAmount[i];
				view.players[unitStack.factionOwner]->addSpice(tax);
				std::cout << "      " << view.players[unitStack.factionOwner]->getFactionName()
				          << " collects " << tax << " spice tax from " << cities[i] << std::endl;
			}
		}
	}
}

void SpiceCollectionPhase::collectSpiceFromTerritories(PhaseContext::SpiceCollectionView& view) {
	std::cout << "    Unit Spice Collection:" << std::endl;
	
	for (const auto& territory : view.map.getTerritories()) {
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
			view.players[unitStack.factionOwner]->addSpice(spiceCollected);
			
			// Remove from territory
			view.map.getTerritory(territory.name)->spiceAmount -= spiceCollected;

			std::cout << "      " << view.players[unitStack.factionOwner]->getFactionName()
			          << " collects " << spiceCollected << " spice from " << territory.name
			          << " (" << view.players[unitStack.factionOwner]->getSpice() << " total)" << std::endl;
		}
	}
}
