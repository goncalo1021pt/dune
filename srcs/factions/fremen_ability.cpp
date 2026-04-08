#include "factions/fremen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "logger/event_logger.hpp"
#include "map.hpp"
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>
#include <string>

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

bool FremenAbility::canRideWorm() const {
	return true;  // Fremen can ride sandworms
}

bool FremenAbility::onWormHitsTerritory(PhaseContext& ctx, const std::string& territoryName) {
	// Fremen can ride the worm to relocate their units to any other territory
	
	territory* wormTerritory = ctx.map.getTerritory(territoryName);
	if (wormTerritory == nullptr) {
		return false;
	}

	// Find Fremen player
	int fremenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Fremen") {
			fremenIndex = i;
			break;
		}
	}

	if (fremenIndex < 0) {
		return false;
	}

	// Count Fremen units in this territory
	int fremenUnitsHere = 0;
	for (const auto& stack : wormTerritory->unitsPresent) {
		if (stack.factionOwner == fremenIndex) {
			fremenUnitsHere += stack.normal_units + stack.elite_units;
		}
	}

	if (fremenUnitsHere <= 0) {
		return false;  // No Fremen units to ride the worm
	}

	// Remove Fremen units from this territory before worm kills anything
	for (auto it = wormTerritory->unitsPresent.begin(); it != wormTerritory->unitsPresent.end(); ++it) {
		if (it->factionOwner == fremenIndex) {
			it->normal_units = 0;
			it->elite_units = 0;
		}
	}

	// Clean up empty stacks
	wormTerritory->unitsPresent.erase(
		std::remove_if(wormTerritory->unitsPresent.begin(), wormTerritory->unitsPresent.end(),
			[](const unitStack& stack) { return stack.normal_units == 0 && stack.elite_units == 0; }),
		wormTerritory->unitsPresent.end()
	);

	if (ctx.logger) {
		ctx.logger->logDebug("[WORM RIDING] Fremen ride worm from " + territoryName + 
			" with " + std::to_string(fremenUnitsHere) + " units");
	}

	// Choose destination - interactive or AI
	std::string destinationTerritory;
	
	if (ctx.interactiveMode) {
		// Interactive: ask Fremen player where to go
		if (ctx.logger) {
			ctx.logger->logDebug("=== FREMEN WORM RIDING ===");
			ctx.logger->logDebug("Choose destination territory for " + std::to_string(fremenUnitsHere) + " units");
			ctx.logger->logDebug("Valid territories (not in storm):");
		}
		
		// Build list of valid destination territories (only restriction: not in storm)
		std::vector<std::string> validDests;
		const auto& allTerritories = ctx.map.getTerritories();
		for (const auto& terr : allTerritories) {
			// Can't land in storm
			bool inStorm = false;
			for (int sectorNum : terr.sectors) {
				if (sectorNum == ctx.stormSector) {
					inStorm = true;
					break;
				}
			}
			if (inStorm) continue;
			
			validDests.push_back(terr.name);
		}
		
		// Show options
		if (ctx.logger) {
			for (size_t i = 0; i < validDests.size(); ++i) {
				ctx.logger->logDebug(std::to_string(i) + ". " + validDests[i]);
			}
		}
		
		// Get player choice
		bool validChoice = false;
		while (!validChoice) {
			if (ctx.logger) {
				ctx.logger->logDebug("Enter territory number or name:");
			}
			std::cout << "> ";
			std::getline(std::cin, destinationTerritory);
			
			// Check if it's a number
			try {
				size_t idx = std::stoi(destinationTerritory);
				if (idx < validDests.size()) {
					destinationTerritory = validDests[idx];
					validChoice = true;
				} else {
					if (ctx.logger) {
						ctx.logger->logDebug("[ERROR] Invalid index. Try again.");
					}
				}
			} catch (...) {
				// Not a number, treat as territory name
				for (const auto& validDest : validDests) {
					if (validDest == destinationTerritory) {
						validChoice = true;
						break;
					}
				}
				
				if (!validChoice) {
					if (ctx.logger) {
						ctx.logger->logDebug("[ERROR] Territory not in valid list. Try again.");
					}
				}
			}
		}
	} else {
		// AI: choose first valid territory
		std::vector<std::string> validDests;
		const auto& allTerritories = ctx.map.getTerritories();
		for (const auto& terr : allTerritories) {
			bool inStorm = false;
			for (int sectorNum : terr.sectors) {
				if (sectorNum == ctx.stormSector) {
					inStorm = true;
					break;
				}
			}
			if (inStorm) continue;
			
			validDests.push_back(terr.name);
		}
		
		if (!validDests.empty()) {
			destinationTerritory = validDests[0];
		}
	}

	// Place units at destination
	if (!destinationTerritory.empty()) {
		// Deploy to chosen territory
		ctx.map.addUnitsToTerritory(destinationTerritory, fremenIndex, fremenUnitsHere, 0);
		if (ctx.logger) {
			ctx.logger->logDebug("Fremen units deployed to " + destinationTerritory + " via worm");
		}
	}

	return true;  // Fremen units were evacuated
}

std::vector<std::string> FremenAbility::getValidDeploymentTerritories(PhaseContext& ctx) const {
	// Fremen can only deploy within 2 tiles of Great Flat
	std::vector<std::string> validTerritories;
	
	const territory* source = ctx.map.getTerritory("The Great Flat");
	if (source == nullptr) {
		return validTerritories;  // Great Flat not found, no valid deployment
	}
	
	// BFS to find all territories within 2 distance
	std::queue<std::pair<const territory*, int>> queue;  // {territory, distance}
	std::set<std::string> visited;
	
	queue.push({source, 0});
	visited.insert(source->name);
	
	while (!queue.empty()) {
		auto [currentTerr, distance] = queue.front();
		queue.pop();
		
		// Add to valid list only if distance > 0 (exclude source) and not Polar Sink
		if (distance > 0 && currentTerr->terrain != terrainType::northPole) {
			validTerritories.push_back(currentTerr->name);
		}
		
		// Don't explore beyond distance 2
		if (distance >= 2) {
			continue;
		}
		
		// Explore neighbors
		for (const territory* neighbor : currentTerr->neighbourPtrs) {
			if (visited.find(neighbor->name) == visited.end()) {
				visited.insert(neighbor->name);
				queue.push({neighbor, distance + 1});
			}
		}
	}
	
	return validTerritories;
}

int FremenAbility::getShipmentCost(const territory* terr, int unitCount) const {
	(void)terr;
	(void)unitCount;
	return 0;
}

void FremenAbility::setupAtStart(Player* player) {
	if (player == nullptr) 
        return;
	player->setSpice(3);
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
	
	const std::string territories[] = {"Sietch Tabr", "False Wall South", "False Wall West"};
	int distribution[3] = {0, 0, 0};
	int totalToAllocate = 10;
	
	if (ctx.logger) {
		ctx.logger->logDebug("=== FREMEN STARTING FORCE DEPLOYMENT ===");
		ctx.logger->logDebug("Fremen must distribute 10 starting units across 3 sietches:");
		ctx.logger->logDebug("  1. Sietch Tabr");
		ctx.logger->logDebug("  2. False Wall South");
		ctx.logger->logDebug("  3. False Wall West");
	}
	
	// Check if interactive mode
	if (!ctx.interactiveMode) {
		// AI mode: distribute units evenly with slight preference for Sietch Tabr
		distribution[0] = 4;  // Sietch Tabr (central stronghold)
		distribution[1] = 3;  // False Wall South
		distribution[2] = 3;  // False Wall West
		
		if (ctx.logger) {
			ctx.logger->logDebug("[AI] Fremen AI chosen distribution:");
			ctx.logger->logDebug("  Sietch Tabr: " + std::to_string(distribution[0]) + " units");
			ctx.logger->logDebug("  False Wall South: " + std::to_string(distribution[1]) + " units");
			ctx.logger->logDebug("  False Wall West: " + std::to_string(distribution[2]) + " units");
		}
	} else {
		// Interactive mode: ask player for distribution
		if (ctx.logger) {
			ctx.logger->logDebug("PLAYER INPUT REQUIRED:");
			ctx.logger->logDebug("Enter distribution for each territory (3 numbers separated by spaces)");
			ctx.logger->logDebug("Must sum to exactly 10. Example: '5 3 2' or '4 3 3'");
		}
		
		bool valid = false;
		while (!valid) {
			std::cout << "> ";
			
			std::string input;
			std::getline(std::cin, input);
			
			int t1, t2, t3;
			try {
				size_t pos1 = input.find(' ');
				size_t pos2 = input.find(' ', pos1 + 1);
				
				if (pos1 == std::string::npos || pos2 == std::string::npos) {
					if (ctx.logger) {
						ctx.logger->logDebug("[ERROR] Invalid format. Enter three numbers separated by spaces.");
					}
					continue;
				}
				
				t1 = std::stoi(input.substr(0, pos1));
				t2 = std::stoi(input.substr(pos1 + 1, pos2 - pos1 - 1));
				t3 = std::stoi(input.substr(pos2 + 1));
				
				if (t1 < 0 || t2 < 0 || t3 < 0) {
					if (ctx.logger) {
						ctx.logger->logDebug("[ERROR] Cannot have negative units. Try again.");
					}
					continue;
				}
				
				if (t1 + t2 + t3 != totalToAllocate) {
					if (ctx.logger) {
						ctx.logger->logDebug("[ERROR] Sum must equal 10. You entered: " + std::to_string(t1 + t2 + t3));
					}
					continue;
				}
				
				distribution[0] = t1;
				distribution[1] = t2;
				distribution[2] = t3;
				valid = true;
				
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("[ERROR] Invalid input. Please enter three numbers.");
				}
			}
		}
	}
	
	// Deploy units to chosen territories
	for (int i = 0; i < 3; ++i) {
		if (distribution[i] > 0) {
			ctx.map.addUnitsToTerritory(territories[i], fremenIndex, distribution[i], 0);
			fremen->deployUnits(distribution[i]);
		}
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Fremen deployment confirmed:");
		ctx.logger->logDebug("  " + territories[0] + ": " + std::to_string(distribution[0]) + " units");
		ctx.logger->logDebug("  " + territories[1] + ": " + std::to_string(distribution[1]) + " units");
		ctx.logger->logDebug("  " + territories[2] + ": " + std::to_string(distribution[2]) + " units");
	}
}
