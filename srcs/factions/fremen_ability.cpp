#include "factions/fremen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "logger/event_logger.hpp"
#include "map.hpp"
#include <queue>
#include <set>
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
