#include "interactive_input.hpp"
#include "phases/phase_context.hpp"
#include "player.hpp"
#include "map.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <set>

InteractiveInput::DeploymentChoice InteractiveInput::getDeploymentDecision(
	PhaseContext& ctx,
	Player* player,
	const std::vector<std::string>& validTargets) {

	displayGameState(ctx, player);
	
	// Territory selection loop - keep asking until valid or skip
	std::string territoryChoice;
	bool validTerritorySelected = false;
	std::string input;
	
	do {
		displayDeploymentMenu(validTargets, player);
		
		std::cin >> input;
		
		// Check for skip commands
		if (input == "0" || input == "skip" || input == "Skip") {
			std::cout << "  Shipment: Skipped" << std::endl;
			return {.territoryName = "", .normalUnits = 0, .eliteUnits = 0, .sector = -1, .shouldDeploy = false};
		}
		
		// Try to parse as number first
		try {
			int index = std::stoi(input);
			if (index >= 1 && index <= static_cast<int>(validTargets.size())) {
				territoryChoice = validTargets[index - 1];  // Convert to 0-based index
				validTerritorySelected = true;
			} else if (index == 0) {
				std::cout << "  Shipment: Skipped" << std::endl;
				return {.territoryName = "", .normalUnits = 0, .eliteUnits = 0, .sector = -1, .shouldDeploy = false};
			} else {
				std::cout << "  Invalid number (" << index << "). Please enter 1-" << validTargets.size() 
				          << " or '0/skip' to abort: ";
			}
		} catch (...) {
			// Not a number, treat as territory name
			auto it = std::find(validTargets.begin(), validTargets.end(), input);
			if (it != validTargets.end()) {
				territoryChoice = input;
				validTerritorySelected = true;
			} else {
				std::cout << "  Invalid territory '" << input << "'. Please try again or enter '0/skip': ";
			}
		}
	} while (!validTerritorySelected);

	std::cout << "  How many units to deploy? (0-" << (player->getUnitsReserve() + player->getEliteUnitsReserve()) << "): ";
	int totalUnits = 0;
	std::cin >> totalUnits;
	int maxAvailable = player->getUnitsReserve() + player->getEliteUnitsReserve();
	totalUnits = std::max(0, std::min(totalUnits, maxAvailable));

	int eliteUnits = 0;
	int normalUnits = totalUnits;
	
	// Only ask for elite split if player has elite units available AND is deploying units
	if (player->getEliteUnitsReserve() > 0 && totalUnits > 0) {
		int maxEliteForDeployment = std::min(totalUnits, player->getEliteUnitsReserve());
		std::cout << "  How many elite units among these " << totalUnits << "? (0-" << maxEliteForDeployment << "): ";
		std::cin >> eliteUnits;
		eliteUnits = std::max(0, std::min(eliteUnits, maxEliteForDeployment));
		normalUnits = totalUnits - eliteUnits;
	}

	// Ask for destination sector (if multi-sector territory)
	const territory* destTerr = ctx.map.getTerritory(territoryChoice);
	int chosenSector = -1;
	
	// Only skip sector selection for Polar Sink (always storm-safe, not part of sector system)
	if (destTerr && destTerr->terrain == terrainType::northPole) {
		chosenSector = -1;  // Special marker for Polar Sink
	} else if (destTerr && destTerr->sectors.size() > 1) {
		// Multi-sector territory - ask player to choose
		std::cout << "\n  Sectors in " << territoryChoice << ":" << std::endl;
		std::vector<int> safeSectors;
		for (int s : destTerr->sectors) {
			if (GameMap::canLeaveSector(s, ctx.stormSector)) {  // Safe to enter
				int spiceHere = ctx.map.getSpiceInSector(territoryChoice, s);
				std::cout << "  " << (safeSectors.size() + 1) << ". Sector " << s << " (safe, " 
					<< spiceHere << " spice)" << std::endl;
				safeSectors.push_back(s);
			} else {
				int spiceHere = ctx.map.getSpiceInSector(territoryChoice, s);
				std::cout << "  X. Sector " << s << " (IN STORM - cannot deploy, " 
					<< spiceHere << " spice)" << std::endl;
			}
		}
		
		if (!safeSectors.empty()) {
			bool validSectorSelected = false;
			do {
				std::cout << "  Which sector to deploy to? (1-" << safeSectors.size() << "): ";
				std::cin >> input;
				try {
					int sectorIndex = std::stoi(input);
					if (sectorIndex >= 1 && sectorIndex <= static_cast<int>(safeSectors.size())) {
						chosenSector = safeSectors[sectorIndex - 1];
						validSectorSelected = true;
					} else {
						std::cout << "  Invalid sector number. Please enter 1-" << safeSectors.size() << ".\n";
					}
				} catch (...) {
					std::cout << "  Invalid input. Please enter a number.\n";
				}
			} while (!validSectorSelected);
		} else {
			std::cout << "  No safe sectors available (all in storm).\n";
			return {.territoryName = "", .normalUnits = 0, .eliteUnits = 0, .sector = -1, .shouldDeploy = false};
		}
	} else if (destTerr && destTerr->sectors.size() == 1) {
		chosenSector = destTerr->sectors[0];
	}

	return {
		.territoryName = territoryChoice,
		.normalUnits = normalUnits,
		.eliteUnits = eliteUnits,
		.sector = chosenSector,
		.shouldDeploy = (normalUnits + eliteUnits) > 0
	};
}

InteractiveInput::MovementChoice InteractiveInput::getMovementDecision(
	PhaseContext& ctx,
	Player* player,
	const std::vector<std::string>& territoriesWithUnits,
	int movementRange) {

	displayGameState(ctx, player);
	
	std::cout << "\n  === Movement Phase ===" << std::endl;
	std::cout << "  Movement range: " << movementRange << std::endl;
	
	// Select source territory
	std::cout << "\n  Territories with your units:" << std::endl;
	std::cout << "  0. Skip" << std::endl;
	for (size_t i = 0; i < territoriesWithUnits.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << territoriesWithUnits[i] << std::endl;
	}
	
	std::string fromTerritory;
	bool validFromSelected = false;
	std::string input;
	std::string toTerritory;
	
	do {
		std::cout << "  Select territory to move FROM (0 to skip): ";
		std::cin >> input;
		
		if (input == "0" || input == "skip" || input == "Skip") {
			std::cout << "  Movement: Skipped" << std::endl;
			return {.fromTerritory = "", .toTerritory = "", .normalUnits = 0, .eliteUnits = 0, .fromSector = -1, .toSector = -1, .shouldMove = false};
		}
		
		try {
			int index = std::stoi(input);
			if (index >= 1 && index <= static_cast<int>(territoriesWithUnits.size())) {
				fromTerritory = territoriesWithUnits[index - 1];
				validFromSelected = true;
			} else {
				std::cout << "  Invalid number. Please enter 1-" << territoriesWithUnits.size() << " or 0 to skip.\n";
			}
		} catch (...) {
			auto it = std::find(territoriesWithUnits.begin(), territoriesWithUnits.end(), input);
			if (it != territoriesWithUnits.end()) {
				fromTerritory = input;
				validFromSelected = true;
			} else {
				std::cout << "  Invalid territory. Please try again.\n";
			}
		}
	} while (!validFromSelected);
	
	// Compute reachable territories from source using BFS
	std::vector<std::string> reachableTerritories;
	{
		const territory* source = ctx.map.getTerritory(fromTerritory);
		if (source != nullptr) {
			std::queue<std::pair<const territory*, int>> bfsQueue;
			std::set<std::string> visited;
			
			bfsQueue.push({source, 0});
			visited.insert(source->name);
			
			while (!bfsQueue.empty()) {
				auto [currentTerr, distance] = bfsQueue.front();
				bfsQueue.pop();
				
				// Add to reachable (exclude source)
				if (distance > 0) {
					reachableTerritories.push_back(currentTerr->name);
				}
				
				// Don't explore beyond movement range
				if (distance >= movementRange) {
					continue;
				}
				
				// Explore neighbors
				for (const territory* neighbor : currentTerr->neighbourPtrs) {
					if (visited.find(neighbor->name) == visited.end()) {
						visited.insert(neighbor->name);
						bfsQueue.push({neighbor, distance + 1});
					}
				}
			}
		}
	}
	
	if (reachableTerritories.empty()) {
		std::cout << "  No reachable destinations from " << fromTerritory << std::endl;
		return {.fromTerritory = "", .toTerritory = "", .normalUnits = 0, .eliteUnits = 0, .fromSector = -1, .toSector = -1, .shouldMove = false};
	}
	
	// Display reachable territories menu
	std::cout << "\n  Reachable territories from " << fromTerritory << ":" << std::endl;
	std::cout << "  0. Cancel" << std::endl;
	for (size_t i = 0; i < reachableTerritories.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << reachableTerritories[i] << std::endl;
	}
	
	// Let player select destination
	bool validToSelected = false;
	do {
		std::cout << "  Select territory to move TO (0 to cancel) or enter name: ";
		std::cin >> input;
		
		if (input == "0" || input == "cancel" || input == "Cancel") {
			return {.fromTerritory = "", .toTerritory = "", .normalUnits = 0, .eliteUnits = 0, .fromSector = -1, .toSector = -1, .shouldMove = false};
		}
		
		// Try number first
		try {
			int index = std::stoi(input);
			if (index >= 1 && index <= static_cast<int>(reachableTerritories.size())) {
				toTerritory = reachableTerritories[index - 1];
				validToSelected = true;
			} else {
				std::cout << "  Invalid number. Please enter 1-" << reachableTerritories.size() << " or 0 to cancel.\n";
			}
		} catch (...) {
			// Try as territory name
			auto it = std::find(reachableTerritories.begin(), reachableTerritories.end(), input);
			if (it != reachableTerritories.end()) {
				toTerritory = input;
				validToSelected = true;
			} else {
				std::cout << "  Invalid territory. Please try again.\n";
			}
		}
	} while (!validToSelected);
	
	// Ask for source sector (if multi-sector territory)
	const territory* fromTerr = ctx.map.getTerritory(fromTerritory);
	int fromSector = -1;
	if (fromTerr && fromTerr->sectors.size() > 1) {
		std::cout << "\n  Sectors in " << fromTerritory << " with your units:" << std::endl;
		std::vector<int> sourceSectors;
		for (int s : fromTerr->sectors) {
			int unitsHere = ctx.map.getUnitsInTerritorySector(fromTerritory, player->getFactionIndex(), s);
			if (unitsHere > 0 && GameMap::canLeaveSector(s, ctx.stormSector)) {
				std::cout << "  " << (sourceSectors.size() + 1) << ". Sector " << s 
					<< " (" << unitsHere << " units, safe)" << std::endl;
				sourceSectors.push_back(s);
			} else if (unitsHere > 0) {
				std::cout << "  X. Sector " << s << " (" << unitsHere << " units, IN STORM - cannot leave)" << std::endl;
			}
		}
		
		if (!sourceSectors.empty()) {
			bool validSectorSelected = false;
			do {
				std::cout << "  Which sector to move FROM? (1-" << sourceSectors.size() << "): ";
				std::cin >> input;
				try {
					int sectorIndex = std::stoi(input);
					if (sectorIndex >= 1 && sectorIndex <= static_cast<int>(sourceSectors.size())) {
						fromSector = sourceSectors[sectorIndex - 1];
						validSectorSelected = true;
					} else {
						std::cout << "  Invalid sector number. Please enter 1-" << sourceSectors.size() << ".\n";
					}
				} catch (...) {
					std::cout << "  Invalid input. Please enter a number.\n";
				}
			} while (!validSectorSelected);
		} else {
			std::cout << "  No units can leave any sector (all in storm).\n";
			return {.fromTerritory = "", .toTerritory = "", .normalUnits = 0, .eliteUnits = 0, 
					.fromSector = -1, .toSector = -1, .shouldMove = false};
		}
	} else if (fromTerr && fromTerr->sectors.size() == 1) {
		fromSector = fromTerr->sectors[0];
	}
	
	// Get available units in source sector to validate movement
	int unitsInSourceSector = ctx.map.getUnitsInTerritorySector(fromTerritory, player->getFactionIndex(), fromSector);
	int eliteInSourceSector = ctx.map.getEliteUnitsInTerritorySector(fromTerritory, player->getFactionIndex(), fromSector);
	int normalInSourceSector = unitsInSourceSector - eliteInSourceSector;
	
	// Ask how many total units to move from this sector
	std::cout << "\n  Total units in this sector: " << unitsInSourceSector << " (" 
		<< normalInSourceSector << " normal, " << eliteInSourceSector << " elite)" << std::endl;
	std::cout << "  How many units to move? (0-" << unitsInSourceSector << "): ";
	int totalUnits = 0;
	std::cin >> totalUnits;
	totalUnits = std::max(0, std::min(totalUnits, unitsInSourceSector));
	
	int normalUnits = totalUnits;
	int eliteUnits = 0;
	
	// If there are elite units in the stack, ask how many should be elite
	if (eliteInSourceSector > 0 && totalUnits > 0) {
		int maxEliteForMovement = std::min(totalUnits, eliteInSourceSector);
		int minEliteForMovement = std::max(0, totalUnits - normalInSourceSector);
		std::cout << "  How many elite among these " << totalUnits << "? (" << minEliteForMovement
			<< "-" << maxEliteForMovement << "): ";
		std::cin >> eliteUnits;
		eliteUnits = std::max(minEliteForMovement, std::min(eliteUnits, maxEliteForMovement));
		normalUnits = totalUnits - eliteUnits;
	}
	
	// Ask for destination sector (if multi-sector territory)
	const territory* toTerr = ctx.map.getTerritory(toTerritory);
	int toSector = -1;
	
	// Only skip sector selection for Polar Sink (always storm-safe, not part of sector system)
	if (toTerr && toTerr->terrain == terrainType::northPole) {
		toSector = -1;  // Special marker for Polar Sink
	} else if (toTerr && toTerr->sectors.size() > 1) {
		// Multi-sector territory - ask player to choose
		std::cout << "\n  Sectors in " << toTerritory << ":" << std::endl;
		std::vector<int> destSectors;
		for (int s : toTerr->sectors) {
			if (GameMap::canLeaveSector(s, ctx.stormSector)) {  // Safe to enter
				int spiceHere = ctx.map.getSpiceInSector(toTerritory, s);
				std::cout << "  " << (destSectors.size() + 1) << ". Sector " << s << " (safe, " 
					<< spiceHere << " spice)" << std::endl;
				destSectors.push_back(s);
			} else {
				int spiceHere = ctx.map.getSpiceInSector(toTerritory, s);
				std::cout << "  X. Sector " << s << " (IN STORM - cannot enter, " 
					<< spiceHere << " spice)" << std::endl;
			}
		}
		
		if (!destSectors.empty()) {
			bool validSectorSelected = false;
			do {
				std::cout << "  Which sector to move TO? (1-" << destSectors.size() << "): ";
				std::cin >> input;
				try {
					int sectorIndex = std::stoi(input);
					if (sectorIndex >= 1 && sectorIndex <= static_cast<int>(destSectors.size())) {
						toSector = destSectors[sectorIndex - 1];
						validSectorSelected = true;
					} else {
						std::cout << "  Invalid sector number. Please enter 1-" << destSectors.size() << ".\n";
					}
				} catch (...) {
					std::cout << "  Invalid input. Please enter a number.\n";
				}
			} while (!validSectorSelected);
		} else {
			std::cout << "  No sectors available (all in storm).\n";
			return {.fromTerritory = "", .toTerritory = "", .normalUnits = 0, .eliteUnits = 0, 
					.fromSector = -1, .toSector = -1, .shouldMove = false};
		}
	} else if (toTerr && toTerr->sectors.size() == 1) {
		toSector = toTerr->sectors[0];
	}
	
	return {
		.fromTerritory = fromTerritory,
		.toTerritory = toTerritory,
		.normalUnits = normalUnits,
		.eliteUnits = eliteUnits,
		.fromSector = fromSector,
		.toSector = toSector,
		.shouldMove = (normalUnits + eliteUnits) > 0
	};
}

void InteractiveInput::displayGameState(PhaseContext& /* ctx */, Player* player) {
	std::cout << "\n  === Game State ===" << std::endl;
	std::cout << "  Player: " << player->getFactionName() << std::endl;
	std::cout << "  Spice: " << player->getSpice() << std::endl;
	std::cout << "  Units in reserve: " << player->getUnitsReserve() << std::endl;
	std::cout << "  Units deployed: " << player->getUnitsDeployed() << std::endl;
}

void InteractiveInput::displayDeploymentMenu(
	const std::vector<std::string>& validTargets,
	Player* /* player */) {
	std::cout << "\n  Valid deployment targets:" << std::endl;
	std::cout << "  0. Skip" << std::endl;
	for (size_t i = 0; i < validTargets.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << validTargets[i] << std::endl;
	}
	std::cout << "  Enter territory number (0 to skip) or name: ";
}

int InteractiveInput::showMenuAndGetChoice(const std::vector<std::string>& options) {
	for (size_t i = 0; i < options.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << options[i] << std::endl;
	}
	
	int choice = 0;
	std::cin >> choice;
	
	if (choice < 1 || choice > static_cast<int>(options.size())) {
		return -1;
	}
	
	return choice - 1;
}
