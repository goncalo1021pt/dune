#include "factions/fremen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "logger/event_logger.hpp"
#include "map.hpp"
#include "interaction/interaction_adapter.hpp"
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>
#include <string>

std::string FremenAbility::getFactionName() const {
	return "Fremen";
}

int FremenAbility::getFreeRevivalsPerTurn() const {
	return 3;
}

bool FremenAbility::canBuyAdditionalRevivals() const {
	return false;
}

int FremenAbility::getBaseMovementRange() const {
	return 2;
}

bool FremenAbility::requiresSpiceForFullUnitStrength() const {
	return false;
}

bool FremenAbility::survivesWorm() const {
	return true;
}

bool FremenAbility::hasReducedStormLosses() const {
	return true;
}

bool FremenAbility::canRideWorm() const {
	return true;
}

bool FremenAbility::onWormHitsTerritory(PhaseContext& ctx, const std::string& territoryName) {
	territory* wormTerritory = ctx.map.getTerritory(territoryName);
	if (wormTerritory == nullptr) {
		return false;
	}

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

	int fremenUnitsHere = 0;
	for (const auto& stack : wormTerritory->unitsPresent) {
		if (stack.factionOwner == fremenIndex) {
			fremenUnitsHere += stack.normal_units + stack.elite_units;
		}
	}

	if (fremenUnitsHere <= 0) {
		return false;
	}

	for (auto it = wormTerritory->unitsPresent.begin(); it != wormTerritory->unitsPresent.end(); ++it) {
		if (it->factionOwner == fremenIndex) {
			it->normal_units = 0;
			it->elite_units = 0;
		}
	}

	wormTerritory->unitsPresent.erase(
		std::remove_if(wormTerritory->unitsPresent.begin(), wormTerritory->unitsPresent.end(),
			[](const unitStack& stack) { return stack.normal_units == 0 && stack.elite_units == 0; }),
		wormTerritory->unitsPresent.end()
	);

	if (ctx.logger) {
		ctx.logger->logDebug("[WORM RIDING] Fremen ride worm from " + territoryName + 
			" with " + std::to_string(fremenUnitsHere) + " units");
	}

	std::string destinationTerritory;
	{
		std::vector<std::string> validDests;
		const auto& allTerritories = ctx.map.getTerritories();
		for (const auto& terr : allTerritories) {
			if (!GameMap::canEnterTerritory(&terr, ctx.stormSector)) continue;
			if (!ctx.map.canAddFactionToTerritory(terr.name, fremenIndex)) continue;
			validDests.push_back(terr.name);
		}

		if (ctx.adapter && !validDests.empty()) {
			DecisionRequest req;
			req.kind = "select";
			req.actor_index = fremenIndex;
			req.prompt = "=== FREMEN WORM RIDING === Choose destination for " +
				std::to_string(fremenUnitsHere) + " units:";
			req.options = validDests;
			auto resp = ctx.adapter->requestDecision(req);
			if (resp && resp->valid && !resp->payload_json.empty()) {
				destinationTerritory = resp->payload_json;
			}
		} else if (!validDests.empty()) {
			destinationTerritory = validDests[0];
		}
	}

	// Place units at destination
	if (!destinationTerritory.empty()) {
		const territory* destination = ctx.map.getTerritory(destinationTerritory);
		int safeSector = GameMap::firstSafeSector(destination, ctx.stormSector);
		if (safeSector < 0 || !ctx.map.canAddFactionToTerritory(destinationTerritory, fremenIndex)) {
			if (ctx.logger) {
				ctx.logger->logDebug("[WORM RIDING] Destination became invalid. Worm ride canceled.");
			}
			return false;
		}

		ctx.map.addUnitsToTerritory(destinationTerritory, fremenIndex, fremenUnitsHere, 0, safeSector);
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
		
		// Add to valid list only if distance > 0 (exclude source)
		if (distance > 0) {
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
	// Special units are configured in placeStartingForces based on feature settings.
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

	const bool useAdvancedFactionRules = ctx.featureSettings.advancedFactionAbilities;
	fremen->setEliteUnitsReserve(useAdvancedFactionRules ? 3 : 0);
	
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
	
	if (ctx.adapter) {
		// Interactive mode via adapter: ask player for each territory one by one
		if (ctx.logger) {
			ctx.logger->logDebug("PLAYER INPUT REQUIRED:");
			ctx.logger->logDebug("Distribute starting units across each sietch, specifying normal and elite units.");
		}

		int eliteRemaining = fremen->getEliteUnitsReserve();

		for (int i = 0; i < 3; ++i) {
			int remaining = totalToAllocate;
			for (int j = 0; j < i; ++j) remaining -= distribution[j];

			DecisionRequest tReq;
			tReq.kind = "int";
			tReq.actor_index = fremenIndex;
			tReq.prompt = "Deploying to " + territories[i] + " (remaining: " +
				std::to_string(remaining) + "). How many units to deploy here?";
			tReq.int_min = 0;
			tReq.int_max = remaining;
			int unitsForTerritory = 0;
			auto tResp = ctx.adapter->requestDecision(tReq);
			if (tResp && tResp->valid) {
				try { unitsForTerritory = std::stoi(tResp->payload_json); } catch (...) {}
			}
			distribution[i] = unitsForTerritory;

			int eliteForTerritory = 0;
			if (unitsForTerritory > 0 && eliteRemaining > 0) {
				int maxElite = std::min(unitsForTerritory, eliteRemaining);
				DecisionRequest eReq;
				eReq.kind = "int";
				eReq.actor_index = fremenIndex;
				eReq.prompt = "How many elite (Fedaykin) among these " + std::to_string(unitsForTerritory) + "?";
				eReq.int_min = 0;
				eReq.int_max = maxElite;
				auto eResp = ctx.adapter->requestDecision(eReq);
				if (eResp && eResp->valid) {
					try { eliteForTerritory = std::stoi(eResp->payload_json); } catch (...) {}
				}
				eliteRemaining -= eliteForTerritory;
			}

			if (ctx.logger) {
				ctx.logger->logDebug("  " + territories[i] + ": " + std::to_string(unitsForTerritory) +
					" units (" + std::to_string(unitsForTerritory - eliteForTerritory) + " normal, " +
					std::to_string(eliteForTerritory) + " elite)");
			}

			if (unitsForTerritory > 0) {
				int normalUnits = unitsForTerritory - eliteForTerritory;
				ctx.map.addUnitsToTerritory(territories[i], fremenIndex, normalUnits, eliteForTerritory, -1);
				fremen->deployUnits(unitsForTerritory);
			}
		}
		fremen->setEliteUnitsReserve(eliteRemaining);
	} else {
		// AI mode: distribute units evenly with slight preference for Sietch Tabr
		distribution[0] = 4;
		distribution[1] = 3;
		distribution[2] = 3;

		if (ctx.logger) {
			ctx.logger->logDebug("[AI] Fremen AI chosen distribution:");
			ctx.logger->logDebug("  Sietch Tabr: " + std::to_string(distribution[0]) + " units");
			ctx.logger->logDebug("  False Wall South: " + std::to_string(distribution[1]) + " units");
			ctx.logger->logDebug("  False Wall West: " + std::to_string(distribution[2]) + " units");
		}
	}

	// Deploy units for AI mode (interactive already deployed in loop above)
	if (!ctx.adapter) {
		// AI: distribute special units only in advanced faction mode.
		int eliteDistribution[3] = {0, 0, 0};
		if (useAdvancedFactionRules) {
			eliteDistribution[0] = 1;
			eliteDistribution[1] = 1;
			eliteDistribution[2] = 1;
		}
		
		for (int i = 0; i < 3; ++i) {
			if (distribution[i] > 0) {
				int elite = std::min(eliteDistribution[i], distribution[i]);
				int normal = distribution[i] - elite;
				ctx.map.addUnitsToTerritory(territories[i], fremenIndex, normal, elite, -1);
				fremen->deployUnits(distribution[i]);
			}
		}
		
		// Set elite reserve to 0 (all deployed)
		fremen->setEliteUnitsReserve(0);
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Fremen deployment confirmed:");
		ctx.logger->logDebug("  " + territories[0] + ": " + std::to_string(distribution[0]) + " units");
		ctx.logger->logDebug("  " + territories[1] + ": " + std::to_string(distribution[1]) + " units");
		ctx.logger->logDebug("  " + territories[2] + ": " + std::to_string(distribution[2]) + " units");
	}
}
