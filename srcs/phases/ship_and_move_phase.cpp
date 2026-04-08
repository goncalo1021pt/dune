#include "phases/ship_and_move_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include "player.hpp"
#include "interactive_input.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <set>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

// ============================================================================
// MAIN PHASE EXECUTION
// ============================================================================

void ShipAndMovePhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("SHIP_AND_MOVE Phase");
	}

	auto view = ctx.getShipAndMoveView();
	
	for (size_t i = 0; i < view.turnOrder.size(); ++i) {
		int playerIndex = view.turnOrder[i];
		Player* player  = view.players[playerIndex];
		if (ctx.logger) {
			ctx.logger->logDebug("--- " + player->getFactionName() + "'s Turn ---");
		}
		
		bool didShipment = executePlayerShipment(ctx, player);
		bool didMovement = executePlayerMovement(ctx, player);
		
		if (!didShipment && !didMovement) {
			if (ctx.logger) {
				ctx.logger->logDebug(player->getFactionName() + " passes (no actions taken)");
			}
		}
	}
}

// ============================================================================
// SHIPMENT (DEPLOYMENT)
// ============================================================================

bool ShipAndMovePhase::executePlayerShipment(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();

	if (player->getUnitsReserve() <= 0) {
		if (ctx.logger) ctx.logger->logDebug("Shipment: No units in reserve");
		return false;
	}
	
	std::vector<std::string> validTargets = getValidDeploymentTargets(ctx, player->getFactionIndex());
	
	DeploymentDecision decision;
	
	if (view.interactiveMode) {
		auto choice = InteractiveInput::getDeploymentDecision(ctx, player, validTargets);
		decision.territoryName = choice.territoryName;
		decision.normalUnits   = choice.normalUnits;
		decision.eliteUnits    = choice.eliteUnits;
		decision.shouldDeploy  = choice.shouldDeploy;
		decision.sector        = choice.sector;  // Use player's sector choice
		decision.spiceCost     = 0;
	} else {
		decision = aiDecideDeployment(ctx, player);
	}
	
	if (!decision.shouldDeploy) {
		if (ctx.logger) ctx.logger->logDebug("Shipment: Skipped");
		return false;
	}

	// Resolve sector if not set (AI mode or invalid choice)
	if (decision.sector == -1) {
		const territory* dest = view.map.getTerritory(decision.territoryName);
		decision.sector = GameMap::firstSafeSector(dest, ctx.stormSector);
	}
	
	return deployUnits(ctx, player, decision.territoryName,
	                   decision.normalUnits, decision.eliteUnits, decision.sector);
}

int ShipAndMovePhase::calculateDeploymentCost(const territory* terr, int unitCount, Player* player) const {
	if (terr == nullptr || unitCount <= 0 || player == nullptr) return 0;
	
	FactionAbility* ability = player->getFactionAbility();
	if (ability) return ability->getShipmentCost(terr, unitCount);
	
	if (terr->terrain == terrainType::city) return unitCount;
	return unitCount * 2;
}

std::vector<std::string> ShipAndMovePhase::getValidDeploymentTargets(
	PhaseContext& ctx, int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();
	std::vector<std::string> validTargets;
	
	FactionAbility* ability = ctx.getAbility(factionIndex);
	std::vector<std::string> restricted;
	if (ability) restricted = ability->getValidDeploymentTerritories(ctx);
	
	auto isValid = [&](const territory& terr) -> bool {
		// Cannot ship into a sector that is fully in storm.
		if (!GameMap::canEnterTerritory(&terr, ctx.stormSector)) return false;
		return view.map.canAddFactionToTerritory(terr.name, factionIndex);
	};

	if (!restricted.empty()) {
		for (const auto& name : restricted) {
			const territory* terr = view.map.getTerritory(name);
			if (terr && isValid(*terr)) validTargets.push_back(name);
		}
	} else {
		for (const auto& terr : view.map.getTerritories()) {
			if (isValid(terr)) validTargets.push_back(terr.name);
		}
	}
	
	return validTargets;
}

bool ShipAndMovePhase::deployUnits(PhaseContext& ctx, Player* player,
                                    const std::string& territoryName,
                                    int normalUnits, int eliteUnits, int sector) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	
	if (!isValidDeployment(ctx, player->getFactionIndex(), territoryName, totalUnits)) {
		if (ctx.logger) ctx.logger->logDebug("Deployment FAILED: Invalid deployment");
		return false;
	}
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) return false;
	
	// Resolve sector if not set.
	int effectiveSector = sector;
	if (effectiveSector == -1) {
		effectiveSector = GameMap::firstSafeSector(terr, ctx.stormSector);
	}
	if (effectiveSector == -1) {
		if (ctx.logger) ctx.logger->logDebug("Deployment FAILED: No safe sector available");
		return false;
	}

	int cost = calculateDeploymentCost(terr, totalUnits, player);
	if (player->getSpice() < cost) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Need " + std::to_string(cost) +
				" spice, have " + std::to_string(player->getSpice()));
		}
		return false;
	}
	
	if (player->getUnitsReserve() < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Need " + std::to_string(totalUnits) +
				" units in reserve, have " + std::to_string(player->getUnitsReserve()));
		}
		return false;
	}
	
	player->removeSpice(cost);
	player->deployUnits(totalUnits);
	view.map.addUnitsToTerritory(territoryName, player->getFactionIndex(),
	                              normalUnits, eliteUnits, effectiveSector);
	
	if (ctx.logger) {
		ctx.logger->logDebug("Shipment: " + std::to_string(totalUnits) +
			" units → " + territoryName +
			" (sector " + std::to_string(effectiveSector) + ")" +
			" for " + std::to_string(cost) + " spice");

		Event e(EventType::UNITS_SHIPPED,
			"Deployed " + std::to_string(totalUnits) + " units to " + territoryName,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = player->getFactionName();
		e.territory     = territoryName;
		e.unitCount     = totalUnits;
		e.spiceValue    = cost;
		ctx.logger->logEvent(e);
	}
	
	return true;
}

bool ShipAndMovePhase::isValidDeployment(PhaseContext& ctx, int factionIndex,
                                          const std::string& territoryName,
                                          int unitCount) const {
	auto view = ctx.getShipAndMoveView();

	if (unitCount <= 0) return false;
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) return false;
	if (!GameMap::canEnterTerritory(terr, ctx.stormSector)) return false;
	
	return view.map.canAddFactionToTerritory(territoryName, factionIndex);
}

// ============================================================================
// MOVEMENT
// ============================================================================

bool ShipAndMovePhase::executePlayerMovement(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();

	int movementRange = calculateMovementRange(ctx, player->getFactionIndex());
	
	std::vector<std::string> territoriesWithUnits =
		view.map.getTerritoriesWithUnits(player->getFactionIndex());
	
	if (territoriesWithUnits.empty()) {
		if (ctx.logger) ctx.logger->logDebug("Movement: No units on map");
		return false;
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Movement range: " + std::to_string(movementRange) +
			(movementRange == 3 ? " (ornithopter)" : ""));
	}
	
	MovementDecision decision;
	
	if (view.interactiveMode) {
		auto choice = InteractiveInput::getMovementDecision(ctx, player, territoriesWithUnits, movementRange);
		decision.fromTerritory = choice.fromTerritory;
		decision.toTerritory   = choice.toTerritory;
		decision.normalUnits   = choice.normalUnits;
		decision.eliteUnits    = choice.eliteUnits;
		decision.shouldMove    = choice.shouldMove;
		decision.fromSector    = choice.fromSector;  // Use player's sector choice
		decision.toSector      = choice.toSector;    // Use player's sector choice
	} else {
		decision = aiDecideMovement(ctx, player, movementRange);
	}
	
	if (!decision.shouldMove) {
		if (ctx.logger) ctx.logger->logDebug("Movement: Skipped");
		return false;
	}

	// Resolve sectors if not set by AI/interactive.
	if (decision.fromSector == -1) {
		// Pick any sector of the source that is not in storm.
		const territory* src = view.map.getTerritory(decision.fromTerritory);
		if (src) {
			for (int s : src->sectors) {
				if (GameMap::canLeaveSector(s, ctx.stormSector) &&
				    view.map.getUnitsInTerritorySector(decision.fromTerritory,
				                                        player->getFactionIndex(), s) > 0) {
					decision.fromSector = s;
					break;
				}
			}
		}
	}
	if (decision.toSector == -1) {
		const territory* dest = view.map.getTerritory(decision.toTerritory);
		decision.toSector = GameMap::firstSafeSector(dest, ctx.stormSector);
	}

	if (decision.fromSector == -1 || decision.toSector == -1) {
		if (ctx.logger) ctx.logger->logDebug("Movement FAILED: Could not resolve sectors");
		return false;
	}
	
	return moveUnits(ctx, player->getFactionIndex(),
	                 decision.fromTerritory, decision.fromSector,
	                 decision.toTerritory,   decision.toSector,
	                 decision.normalUnits,   decision.eliteUnits);
}

int ShipAndMovePhase::calculateMovementRange(PhaseContext& ctx, int factionIndex) const {
	auto view = ctx.getShipAndMoveView();

	FactionAbility* ability = ctx.getAbility(factionIndex);
	int baseRange = ability ? ability->getBaseMovementRange() : 1;

	bool controlsArrakeen = view.map.isControlled("Arrakeen", factionIndex);
	bool controlsCarthag  = view.map.isControlled("Carthag",  factionIndex);
	
	return (controlsArrakeen || controlsCarthag) ? 3 : baseRange;
}

std::vector<std::string> ShipAndMovePhase::getReachableTerritories(
	PhaseContext& ctx,
	const std::string& sourceTerritoryName,
	int sourceUnitSector,
	int movementRange,
	int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();
	std::vector<std::string> reachable;
	
	// Units cannot leave if their sector is in storm.
	if (!GameMap::canLeaveSector(sourceUnitSector, ctx.stormSector)) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement blocked: units in sector " +
				std::to_string(sourceUnitSector) + " cannot leave (storm)");
		}
		return reachable;
	}

	const territory* source = view.map.getTerritory(sourceTerritoryName);
	if (source == nullptr) return reachable;
	
	// BFS: visit territories within movementRange steps.
	// An adjacent territory is passable only if canEnterTerritory() is true.
	std::queue<std::pair<const territory*, int>> queue;
	std::set<std::string> visited;
	
	queue.push({source, 0});
	visited.insert(source->name);
	
	while (!queue.empty()) {
		auto [currentTerr, distance] = queue.front();
		queue.pop();
		
		if (distance > 0 &&
		    GameMap::canEnterTerritory(currentTerr, ctx.stormSector) &&
		    view.map.canAddFactionToTerritory(currentTerr->name, factionIndex)) {
			reachable.push_back(currentTerr->name);
		}
		
		if (distance >= movementRange) continue;
		
		for (const territory* neighbor : currentTerr->neighbourPtrs) {
			if (visited.count(neighbor->name)) continue;
			// Cannot pass through a territory that is entirely in storm.
			// (Units can move through it only if it has a safe sector to traverse.)
			if (!GameMap::canEnterTerritory(neighbor, ctx.stormSector)) {
				visited.insert(neighbor->name);
				continue;  // Blocked — do not explore through this territory.
			}
			visited.insert(neighbor->name);
			queue.push({neighbor, distance + 1});
		}
	}
	
	return reachable;
}

bool ShipAndMovePhase::moveUnits(PhaseContext& ctx, int factionIndex,
                                  const std::string& fromTerritory, int fromSector,
                                  const std::string& toTerritory,   int toSector,
                                  int normalUnits, int eliteUnits) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	
	// Check units are available in the specific source sector.
	int unitsInSector = view.map.getUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	if (unitsInSector < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Need " + std::to_string(totalUnits) +
				" in sector " + std::to_string(fromSector) + " of " + fromTerritory +
				", have " + std::to_string(unitsInSector));
		}
		return false;
	}

	// Validate reachability using the source sector.
	int movementRange = calculateMovementRange(ctx, factionIndex);
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, fromTerritory, fromSector, movementRange, factionIndex);
	
	if (std::find(reachable.begin(), reachable.end(), toTerritory) == reachable.end()) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: " + toTerritory +
				" not reachable from sector " + std::to_string(fromSector) +
				" of " + fromTerritory);
		}
		return false;
	}
	
	// Remove from source sector, add to destination sector.
	view.map.removeUnitsFromTerritorySector(fromTerritory, factionIndex,
	                                         normalUnits, eliteUnits, fromSector);
	view.map.addUnitsToTerritory(toTerritory, factionIndex,
	                              normalUnits, eliteUnits, toSector);
	
	if (ctx.logger) {
		ctx.logger->logDebug("Moved " + std::to_string(totalUnits) +
			" units: " + fromTerritory + "(s" + std::to_string(fromSector) + ")" +
			" → " + toTerritory + "(s" + std::to_string(toSector) + ")");
		
		Event e(EventType::UNITS_MOVED,
			"Moved " + std::to_string(totalUnits) +
			" units from " + fromTerritory + " to " + toTerritory,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = view.players[factionIndex]->getFactionName();
		e.territory     = toTerritory;
		e.unitCount     = totalUnits;
		ctx.logger->logEvent(e);
	}
	
	return true;
}

bool ShipAndMovePhase::isValidMovement(PhaseContext& ctx, int factionIndex,
                                        const std::string& fromTerritory,
                                        int fromSector,
                                        const std::string& toTerritory,
                                        int movementRange) const {
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, fromTerritory, fromSector, movementRange, factionIndex);
	return std::find(reachable.begin(), reachable.end(), toTerritory) != reachable.end();
}

// ============================================================================
// AI DECISIONS
// ============================================================================

ShipAndMovePhase::DeploymentDecision ShipAndMovePhase::aiDecideDeployment(
	PhaseContext& ctx, Player* player) const {
	
	auto view = ctx.getShipAndMoveView();

	DeploymentDecision decision;
	decision.shouldDeploy = false;
	decision.normalUnits  = 0;
	decision.eliteUnits   = 0;
	decision.sector       = -1;
	decision.spiceCost    = 0;
	
	if (player->getSpice() < 4 || player->getUnitsReserve() <= 0) return decision;
	
	std::vector<std::string> validTargets =
		getValidDeploymentTargets(ctx, player->getFactionIndex());
	if (validTargets.empty()) return decision;
	
	// Prefer cities (cheapest), otherwise first valid territory.
	std::string targetTerritory;
	for (const auto& name : validTargets) {
		const territory* terr = view.map.getTerritory(name);
		if (terr && terr->terrain == terrainType::city) {
			targetTerritory = name;
			break;
		}
	}
	if (targetTerritory.empty()) targetTerritory = validTargets[0];
	
	const territory* terr = view.map.getTerritory(targetTerritory);
	if (terr == nullptr) return decision;

	int safeSector = GameMap::firstSafeSector(terr, ctx.stormSector);
	if (safeSector == -1) return decision;  // territory fully in storm
	
	int cost = calculateDeploymentCost(terr, 1, player);
	int maxAffordable = (cost == 0) ? player->getUnitsReserve()
	                                 : player->getSpice() / cost;
	int unitsToDeploy = std::min({3, maxAffordable, player->getUnitsReserve()});
	if (unitsToDeploy <= 0) return decision;
	
	decision.shouldDeploy   = true;
	decision.territoryName  = targetTerritory;
	decision.normalUnits    = unitsToDeploy;
	decision.eliteUnits     = 0;
	decision.sector         = safeSector;
	decision.spiceCost      = calculateDeploymentCost(terr, unitsToDeploy, player);
	
	return decision;
}

ShipAndMovePhase::MovementDecision ShipAndMovePhase::aiDecideMovement(
	PhaseContext& ctx, Player* player, int movementRange) const {
	
	auto view = ctx.getShipAndMoveView();

	MovementDecision decision;
	decision.shouldMove  = false;
	decision.normalUnits = 0;
	decision.eliteUnits  = 0;
	decision.fromSector  = -1;
	decision.toSector    = -1;
	
	std::vector<std::string> territoriesWithUnits =
		view.map.getTerritoriesWithUnits(player->getFactionIndex());
	if (territoriesWithUnits.empty()) return decision;
	
	// Try each source territory until we find one with a movable stack.
	for (const auto& sourceName : territoriesWithUnits) {
		const territory* src = view.map.getTerritory(sourceName);
		if (!src) continue;

		// Find a sector in this territory that has units and can leave.
		int movableSector = -1;
		int unitsToMove   = 0;
		for (const auto& stack : src->unitsPresent) {
			if (stack.factionOwner != player->getFactionIndex()) continue;
			if (!GameMap::canLeaveSector(stack.sector, ctx.stormSector)) continue;
			int count = stack.normal_units + stack.elite_units;
			if (count > 0) {
				movableSector = stack.sector;
				unitsToMove   = std::max(1, count / 2);
				break;
			}
		}
		if (movableSector == -1) continue;

		std::vector<std::string> reachable =
			getReachableTerritories(ctx, sourceName, movableSector, movementRange,
			                        player->getFactionIndex());
		if (reachable.empty()) continue;

		// Prefer spice-rich territory, then city, then first reachable.
		std::string best;
		int bestSpice = -1;
		for (const auto& name : reachable) {
			const territory* t = view.map.getTerritory(name);
			if (t) {
				int spiceHere = view.map.getSpiceInTerritory(name);
				if (spiceHere > bestSpice) {
					bestSpice = spiceHere;
					best = name;
				}
			}
		}
		if (best.empty()) {
			for (const auto& name : reachable) {
				const territory* t = view.map.getTerritory(name);
				if (t && t->terrain == terrainType::city) { best = name; break; }
			}
		}
		if (best.empty()) best = reachable[0];

		const territory* dest = view.map.getTerritory(best);
		int toSector = GameMap::firstSafeSector(dest, ctx.stormSector);
		if (toSector == -1) continue;

		decision.shouldMove    = true;
		decision.fromTerritory = sourceName;
		decision.fromSector    = movableSector;
		decision.toTerritory   = best;
		decision.toSector      = toSector;
		decision.normalUnits   = unitsToMove;
		decision.eliteUnits    = 0;
		return decision;
	}
	
	return decision;
}