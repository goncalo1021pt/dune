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

	// Get view for this phase
	auto view = ctx.getShipAndMoveView();
	
	// Each player takes their turn in order (using turn order from storm calculation)
	for (size_t i = 0; i < view.turnOrder.size(); ++i) {
		int playerIndex = view.turnOrder[i];
		Player* player = view.players[playerIndex];
		if (ctx.logger) {
			ctx.logger->logDebug("--- " + player->getFactionName() + "'s Turn ---");
		}
		
		// 1. Shipment (deployment)
		bool didShipment = executePlayerShipment(ctx, player);
		
		// 2. Movement
		bool didMovement = executePlayerMovement(ctx, player);
		
		if (!didShipment && !didMovement) {
			if (ctx.logger) {
				ctx.logger->logDebug(player->getFactionName() + " passes (no actions taken)");
			}
		}
	}
}

// ============================================================================
// SHIPMENT (DEPLOYMENT) METHODS
// ============================================================================

bool ShipAndMovePhase::executePlayerShipment(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();

	// Check if player has units to deploy
	if (player->getUnitsReserve() <= 0) {
		if (ctx.logger) {
			ctx.logger->logDebug("Shipment: No units in reserve to deploy");
		}
		return false;
	}
	
	// Get valid deployment targets
	std::vector<std::string> validTargets = getValidDeploymentTargets(ctx, player->getFactionIndex());
	
	// Get deployment decision (interactive or AI)
	DeploymentDecision decision;
	
	if (view.interactiveMode) {
		// Interactive mode: get player choice
		auto interactiveChoice = InteractiveInput::getDeploymentDecision(ctx, player, validTargets);
		decision.territoryName = interactiveChoice.territoryName;
		decision.normalUnits = interactiveChoice.normalUnits;
		decision.eliteUnits = interactiveChoice.eliteUnits;
		decision.shouldDeploy = interactiveChoice.shouldDeploy;
		decision.spiceCost = 0; // Will be calculated in deployUnits
	} else {
		// AI mode: use AI decision
		decision = aiDecideDeployment(ctx, player);
	}
	
	if (!decision.shouldDeploy) {
		if (ctx.logger) {
			ctx.logger->logDebug("Shipment: Skipped");
		}
		return false;
	}
	
	// Execute deployment
	bool success = deployUnits(ctx, player, decision.territoryName, 
	                          decision.normalUnits, decision.eliteUnits);
	
	return success;
}

int ShipAndMovePhase::calculateDeploymentCost(const territory* terr, int unitCount, Player* player) const {
	if (terr == nullptr || unitCount <= 0 || player == nullptr) {
		return 0;
	}
	
	FactionAbility* ability = player->getFactionAbility();
	if (ability) {
		return ability->getShipmentCost(terr, unitCount);
	}
	
	// Default: cities 1 spice/unit, others 2 spice/unit
	if (terr->terrain == terrainType::city) {
		return unitCount;
	}
	return unitCount * 2;
}

std::vector<std::string> ShipAndMovePhase::getValidDeploymentTargets(
	PhaseContext& ctx, int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();
	std::vector<std::string> validTargets;
	
	// Check if faction has deployment restrictions
	FactionAbility* ability = ctx.getAbility(factionIndex);
	std::vector<std::string> restrictedTerritories;
	if (ability) {
		restrictedTerritories = ability->getValidDeploymentTerritories(ctx);
	}
	
	// If faction has restrictions, only check those territories
	if (!restrictedTerritories.empty()) {
		for (const auto& terrName : restrictedTerritories) {
			const territory* terr = view.map.getTerritory(terrName);
			if (terr != nullptr && view.map.canAddFactionToTerritory(terrName, factionIndex)) {
				validTargets.push_back(terrName);
			}
		}
	} else {
		// No restrictions: all territories except Polar Sink
		for (const auto& terr : view.map.getTerritories()) {
			// Skip Polar Sink (cannot deploy there directly)
			if (terr.terrain == terrainType::northPole) {
				continue;
			}
			
			// Check if faction can add units to this territory
			if (view.map.canAddFactionToTerritory(terr.name, factionIndex)) {
				validTargets.push_back(terr.name);
			}
		}
	}
	
	return validTargets;
}

bool ShipAndMovePhase::deployUnits(PhaseContext& ctx, Player* player,
                                   const std::string& territoryName, 
                                   int normalUnits, int eliteUnits) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	
	// Validation
	if (!isValidDeployment(ctx, player->getFactionIndex(), territoryName, totalUnits)) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Invalid deployment");
		}
		return false;
	}
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) {
		return false;
	}
	
	// Calculate and check cost
	int cost = calculateDeploymentCost(terr, totalUnits, player);
	if (player->getSpice() < cost) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Insufficient spice (need " 
		          + std::to_string(cost) + ", have " + std::to_string(player->getSpice()) + ")");
		}
		return false;
	}
	
	// Check if player has enough units in reserve
	if (player->getUnitsReserve() < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Insufficient units in reserve (need " 
		          + std::to_string(totalUnits) + ", have " + std::to_string(player->getUnitsReserve()) + ")");
		}
		return false;
	}
	
	// Execute deployment
	player->removeSpice(cost);
	player->deployUnits(totalUnits);
	view.map.addUnitsToTerritory(territoryName, player->getFactionIndex(), 
	                            normalUnits, eliteUnits);
	
	if (ctx.logger) {
		ctx.logger->logDebug("Shipment: Deployed " + std::to_string(totalUnits) + " units to " 
		          + territoryName + " for " + std::to_string(cost) + " spice");
		ctx.logger->logDebug("(" + player->getFactionName() + " now has " 
		          + std::to_string(player->getSpice()) + " spice, " 
		          + std::to_string(player->getUnitsReserve()) + " units in reserve)");
		
		Event e(EventType::UNITS_MOVED,
			"Deployed " + std::to_string(totalUnits) + " units to " + territoryName,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = player->getFactionName();
		e.territory = territoryName;
		e.unitCount = totalUnits;
		e.spiceValue = cost;
		ctx.logger->logEvent(e);
	}
	
	return true;
}

bool ShipAndMovePhase::isValidDeployment(PhaseContext& ctx, int factionIndex,
                                         const std::string& territoryName, 
                                         int unitCount) const {
	auto view = ctx.getShipAndMoveView();

	if (unitCount <= 0) {
		return false;
	}
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) {
		return false;
	}
	
	// Cannot deploy directly to Polar Sink
	if (terr->terrain == terrainType::northPole) {
		return false;
	}
	
	// Check territory occupancy rules
	return view.map.canAddFactionToTerritory(territoryName, factionIndex);
}

// ============================================================================
// MOVEMENT METHODS
// ============================================================================

bool ShipAndMovePhase::executePlayerMovement(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();

	int movementRange = calculateMovementRange(ctx, player->getFactionIndex());
	
	// Get territories where player has units
	std::vector<std::string> territoriesWithUnits = view.map.getTerritoriesWithUnits(player->getFactionIndex());
	
	if (territoriesWithUnits.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement: No units on map to move");
		}
		return false;
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Movement range: " + std::to_string(movementRange) 
		          + (movementRange == 3 ? " (special movement)" : ""));
	}
	
	// Get movement decision (interactive or AI)
	MovementDecision decision;
	
	if (view.interactiveMode) {
		// Interactive mode: get player choice
		auto interactiveChoice = InteractiveInput::getMovementDecision(ctx, player, territoriesWithUnits, movementRange);
		decision.fromTerritory = interactiveChoice.fromTerritory;
		decision.toTerritory = interactiveChoice.toTerritory;
		decision.normalUnits = interactiveChoice.normalUnits;
		decision.eliteUnits = interactiveChoice.eliteUnits;
		decision.shouldMove = interactiveChoice.shouldMove;
	} else {
		// AI mode: use AI decision
		decision = aiDecideMovement(ctx, player, movementRange);
	}
	
	if (!decision.shouldMove) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement: Skipped");
		}
		return false;
	}
	
	// Execute movement
	bool success = moveUnits(ctx, player->getFactionIndex(),
	                        decision.fromTerritory, decision.toTerritory,
	                        decision.normalUnits, decision.eliteUnits);
	
	return success;
}

int ShipAndMovePhase::calculateMovementRange(PhaseContext& ctx, int factionIndex) const {
	auto view = ctx.getShipAndMoveView();

	FactionAbility* ability = ctx.getAbility(factionIndex);
	int baseRange = (ability) ? ability->getBaseMovementRange() : 1;

	// Check if faction controls Arrakeen or Carthag (ornithopter bonus)
	bool controlsArrakeen = view.map.isControlled("Arrakeen", factionIndex);
	bool controlsCarthag = view.map.isControlled("Carthag", factionIndex);
	
	if (controlsArrakeen || controlsCarthag) {
		return 3;  // Ornithopter bonus overrides base range
	}
	
	return baseRange;
}

std::vector<std::string> ShipAndMovePhase::getReachableTerritories(
	PhaseContext& ctx,
	const std::string& sourceTerritoryName,
	int movementRange,
	int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();

	std::vector<std::string> reachable;
	
	const territory* source = view.map.getTerritory(sourceTerritoryName);
	if (source == nullptr) {
		return reachable;
	}
	
	// BFS to find all territories within movement range
	std::queue<std::pair<const territory*, int>> queue;  // {territory, distance}
	std::set<std::string> visited;
	
	queue.push({source, 0});
	visited.insert(source->name);
	
	while (!queue.empty()) {
		auto [currentTerr, distance] = queue.front();
		queue.pop();
		
		// Add to reachable list (excluding source)
		if (distance > 0 && view.map.canAddFactionToTerritory(currentTerr->name, factionIndex)) {
			reachable.push_back(currentTerr->name);
		}
		
		// Don't explore beyond movement range
		if (distance >= movementRange) {
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
	
	return reachable;
}

bool ShipAndMovePhase::moveUnits(PhaseContext& ctx, int factionIndex,
                                 const std::string& fromTerritory,
                                 const std::string& toTerritory,
                                 int normalUnits, int eliteUnits) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	
	// Get movement range for validation
	int movementRange = calculateMovementRange(ctx, factionIndex);
	
	// Validation
	if (!isValidMovement(ctx, factionIndex, fromTerritory, toTerritory, movementRange)) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Invalid movement");
		}
		return false;
	}
	
	// Check if player has enough units in source territory
	int unitsInSource = view.map.getUnitsInTerritory(fromTerritory, factionIndex);
	if (unitsInSource < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Not enough units in " + fromTerritory
		          + " (need " + std::to_string(totalUnits) + ", have " + std::to_string(unitsInSource) + ")");
		}
		return false;
	}
	
	// Execute movement
	view.map.removeUnitsFromTerritory(fromTerritory, factionIndex, normalUnits, eliteUnits);
	view.map.addUnitsToTerritory(toTerritory, factionIndex, normalUnits, eliteUnits);
	
	if (ctx.logger) {
		ctx.logger->logDebug("Movement: Moved " + std::to_string(totalUnits) + " units from " 
		          + fromTerritory + " to " + toTerritory);
		
		Event e(EventType::UNITS_MOVED,
			"Moved " + std::to_string(totalUnits) + " units from " + fromTerritory + " to " + toTerritory,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = view.players[factionIndex]->getFactionName();
		e.territory = toTerritory;
		e.unitCount = totalUnits;
		ctx.logger->logEvent(e);
	}
	
	return true;
}



bool ShipAndMovePhase::isValidMovement(PhaseContext& ctx, int factionIndex,
                                       const std::string& fromTerritory,
                                       const std::string& toTerritory,
                                       int movementRange) const {
	
	// Get reachable territories from source
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, fromTerritory, movementRange, factionIndex);
	
	// Check if destination is reachable
	return std::find(reachable.begin(), reachable.end(), toTerritory) != reachable.end();
}

// ============================================================================
// AI DECISION METHODS (Simple AI for now)
// ============================================================================

ShipAndMovePhase::DeploymentDecision ShipAndMovePhase::aiDecideDeployment(
	PhaseContext& ctx, Player* player) const {
	
	auto view = ctx.getShipAndMoveView();

	DeploymentDecision decision;
	decision.shouldDeploy = false;
	decision.normalUnits = 0;
	decision.eliteUnits = 0;
	decision.spiceCost = 0;
	
	// Simple AI: Don't deploy if we have less than 4 spice (save for emergencies)
	if (player->getSpice() < 4 || player->getUnitsReserve() <= 0) {
		return decision;
	}
	
	// Get valid deployment targets
	std::vector<std::string> validTargets = getValidDeploymentTargets(ctx, player->getFactionIndex());
	
	if (validTargets.empty()) {
		return decision;
	}
	
	// Simple AI: Deploy to first city if possible (cheapest), otherwise first valid territory
	std::string targetTerritory;
	for (const auto& terrName : validTargets) {
		const territory* terr = view.map.getTerritory(terrName);
		if (terr != nullptr && terr->terrain == terrainType::city) {
			targetTerritory = terrName;
			break;
		}
	}
	
	if (targetTerritory.empty()) {
		targetTerritory = validTargets[0];
	}
	
	const territory* terr = view.map.getTerritory(targetTerritory);
	if (terr == nullptr) {
		return decision;
	}
	
	// Deploy up to 3 units, limited by spice and reserve
	int cost = calculateDeploymentCost(terr, 1, player);
	int maxAffordable;
	if (cost == 0) {
		// Free shipping (e.g., Fremen): deploy up to reserve limit
		maxAffordable = player->getUnitsReserve();
	} else {
		maxAffordable = player->getSpice() / cost;
	}
	int unitsToDepl = std::min({3, maxAffordable, player->getUnitsReserve()});
	
	if (unitsToDepl <= 0) {
		return decision;
	}
	
	decision.shouldDeploy = true;
	decision.territoryName = targetTerritory;
	decision.normalUnits = unitsToDepl;
	decision.eliteUnits = 0;
	decision.spiceCost = calculateDeploymentCost(terr, unitsToDepl, player);
	
	return decision;
}

ShipAndMovePhase::MovementDecision ShipAndMovePhase::aiDecideMovement(
	PhaseContext& ctx, Player* player, int movementRange) const {
	
	auto view = ctx.getShipAndMoveView();

	MovementDecision decision;
	decision.shouldMove = false;
	decision.normalUnits = 0;
	decision.eliteUnits = 0;
	
	// Get territories with units
	std::vector<std::string> territoriesWithUnits = view.map.getTerritoriesWithUnits(player->getFactionIndex());
	
	if (territoriesWithUnits.empty()) {
		return decision;
	}
	
	// Simple AI: Move units from first territory with units toward spice-rich territories
	std::string sourceTerritory = territoriesWithUnits[0];
	
	// Get reachable territories
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, sourceTerritory, movementRange, player->getFactionIndex());
	
	if (reachable.empty()) {
		return decision;
	}
	
	// Find territory with most spice
	std::string bestTarget;
	int bestSpice = -1;
	
	for (const auto& terrName : reachable) {
		const territory* terr = view.map.getTerritory(terrName);
		if (terr != nullptr && terr->spiceAmount > bestSpice) {
			bestSpice = terr->spiceAmount;
			bestTarget = terrName;
		}
	}
	
	// If no spice found, move to first reachable city
	if (bestTarget.empty()) {
		for (const auto& terrName : reachable) {
			const territory* terr = view.map.getTerritory(terrName);
			if (terr != nullptr && terr->terrain == terrainType::city) {
				bestTarget = terrName;
				break;
			}
		}
	}
	
	// If still no target, just move to first reachable
	if (bestTarget.empty()) {
		bestTarget = reachable[0];
	}
	
	// Move half of units (at least 1)
	int unitsInSource = view.map.getUnitsInTerritory(sourceTerritory, player->getFactionIndex());
	int unitsToMove = std::max(1, unitsInSource / 2);
	
	decision.shouldMove = true;
	decision.fromTerritory = sourceTerritory;
	decision.toTerritory = bestTarget;
	decision.normalUnits = unitsToMove;
	decision.eliteUnits = 0;
	
	return decision;
}
