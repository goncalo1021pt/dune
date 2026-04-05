#include "phases/ship_and_move_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include "player.hpp"
#include "interactive_input.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <set>

// ============================================================================
// MAIN PHASE EXECUTION
// ============================================================================

void ShipAndMovePhase::execute(PhaseContext& ctx) {
	std::cout << "  SHIP_AND_MOVE Phase" << std::endl;
	
	// Each player takes their turn in order
	for (int i = 0; i < ctx.playerCount; ++i) {
		Player* player = ctx.players[i];
		std::cout << "\n    --- " << player->getFactionName() << "'s Turn ---" << std::endl;
		
		// 1. Shipment (deployment)
		bool didShipment = executePlayerShipment(ctx, player);
		
		// 2. Movement
		bool didMovement = executePlayerMovement(ctx, player);
		
		if (!didShipment && !didMovement) {
			std::cout << "    " << player->getFactionName() << " passes (no actions taken)" << std::endl;
		}
	}
}

// ============================================================================
// SHIPMENT (DEPLOYMENT) METHODS
// ============================================================================

bool ShipAndMovePhase::executePlayerShipment(PhaseContext& ctx, Player* player) {
	// Check if player has units to deploy
	if (player->getUnitsReserve() <= 0) {
		std::cout << "    Shipment: No units in reserve to deploy" << std::endl;
		return false;
	}
	
	// Get valid deployment targets
	std::vector<std::string> validTargets = getValidDeploymentTargets(ctx, player->getFactionIndex());
	
	// Get deployment decision (interactive or AI)
	DeploymentDecision decision;
	
	if (ctx.interactiveMode) {
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
		std::cout << "    Shipment: Skipped" << std::endl;
		return false;
	}
	
	// Execute deployment
	bool success = deployUnits(ctx, player, decision.territoryName, 
	                          decision.normalUnits, decision.eliteUnits);
	
	return success;
}

int ShipAndMovePhase::calculateDeploymentCost(const territory* terr, int unitCount) const {
	if (terr == nullptr || unitCount <= 0) {
		return 0;
	}
	
	// Cities cost 1 spice per unit
	if (terr->terrain == terrainType::city) {
		return unitCount * 1;
	}
	
	// All other territories cost 2 spice per unit
	return unitCount * 2;
}

std::vector<std::string> ShipAndMovePhase::getValidDeploymentTargets(
	PhaseContext& ctx, int factionIndex) const {
	
	std::vector<std::string> validTargets;
	
	for (const auto& terr : ctx.map.getTerritories()) {
		// Skip Polar Sink (cannot deploy there directly)
		if (terr.terrain == terrainType::northPole) {
			continue;
		}
		
		// Check if faction can add units to this territory
		if (ctx.map.canAddFactionToTerritory(terr.name, factionIndex)) {
			validTargets.push_back(terr.name);
		}
	}
	
	return validTargets;
}

bool ShipAndMovePhase::deployUnits(PhaseContext& ctx, Player* player,
                                   const std::string& territoryName, 
                                   int normalUnits, int eliteUnits) {
	
	int totalUnits = normalUnits + eliteUnits;
	
	// Validation
	if (!isValidDeployment(ctx, player->getFactionIndex(), territoryName, totalUnits)) {
		std::cout << "    Deployment FAILED: Invalid deployment" << std::endl;
		return false;
	}
	
	territory* terr = ctx.map.getTerritory(territoryName);
	if (terr == nullptr) {
		return false;
	}
	
	// Calculate and check cost
	int cost = calculateDeploymentCost(terr, totalUnits);
	if (player->getSpice() < cost) {
		std::cout << "    Deployment FAILED: Insufficient spice (need " 
		          << cost << ", have " << player->getSpice() << ")" << std::endl;
		return false;
	}
	
	// Check if player has enough units in reserve
	if (player->getUnitsReserve() < totalUnits) {
		std::cout << "    Deployment FAILED: Insufficient units in reserve (need " 
		          << totalUnits << ", have " << player->getUnitsReserve() << ")" << std::endl;
		return false;
	}
	
	// Execute deployment
	player->removeSpice(cost);
	player->deployUnits(totalUnits);
	ctx.map.addUnitsToTerritory(territoryName, player->getFactionIndex(), 
	                            normalUnits, eliteUnits);
	
	std::cout << "    Shipment: Deployed " << totalUnits << " units to " 
	          << territoryName << " for " << cost << " spice" << std::endl;
	std::cout << "      (" << player->getFactionName() << " now has " 
	          << player->getSpice() << " spice, " 
	          << player->getUnitsReserve() << " units in reserve)" << std::endl;
	
	return true;
}

bool ShipAndMovePhase::isValidDeployment(PhaseContext& ctx, int factionIndex,
                                         const std::string& territoryName, 
                                         int unitCount) const {
	if (unitCount <= 0) {
		return false;
	}
	
	territory* terr = ctx.map.getTerritory(territoryName);
	if (terr == nullptr) {
		return false;
	}
	
	// Cannot deploy directly to Polar Sink
	if (terr->terrain == terrainType::northPole) {
		return false;
	}
	
	// Check territory occupancy rules
	return ctx.map.canAddFactionToTerritory(territoryName, factionIndex);
}

// ============================================================================
// MOVEMENT METHODS
// ============================================================================

bool ShipAndMovePhase::executePlayerMovement(PhaseContext& ctx, Player* player) {
	int movementRange = calculateMovementRange(ctx, player->getFactionIndex());
	
	// Get territories where player has units
	std::vector<std::string> territoriesWithUnits = getTerritoriesWithUnits(ctx, player->getFactionIndex());
	
	if (territoriesWithUnits.empty()) {
		std::cout << "    Movement: No units on map to move" << std::endl;
		return false;
	}
	
	std::cout << "    Movement range: " << movementRange 
	          << (movementRange == 3 ? " (special movement)" : "") << std::endl;
	
	// Get movement decision (interactive or AI)
	MovementDecision decision;
	
	if (ctx.interactiveMode) {
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
		std::cout << "    Movement: Skipped" << std::endl;
		return false;
	}
	
	// Execute movement
	bool success = moveUnits(ctx, player->getFactionIndex(),
	                        decision.fromTerritory, decision.toTerritory,
	                        decision.normalUnits, decision.eliteUnits);
	
	return success;
}

int ShipAndMovePhase::calculateMovementRange(PhaseContext& ctx, int factionIndex) const {
	// Check if faction controls Arrakeen or Carthag
	bool controlsArrakeen = ctx.map.isControlled("Arrakeen", factionIndex);
	bool controlsCarthag = ctx.map.isControlled("Carthag", factionIndex);
	
	if (controlsArrakeen || controlsCarthag) {
		return 3;  // Special movement
	}
	
	return 1;  // Base movement
}

std::vector<std::string> ShipAndMovePhase::getReachableTerritories(
	PhaseContext& ctx,
	const std::string& sourceTerritoryName,
	int movementRange,
	int factionIndex) const {
	
	std::vector<std::string> reachable;
	
	const territory* source = ctx.map.getTerritory(sourceTerritoryName);
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
		if (distance > 0 && ctx.map.canAddFactionToTerritory(currentTerr->name, factionIndex)) {
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
	
	int totalUnits = normalUnits + eliteUnits;
	
	// Get movement range for validation
	int movementRange = calculateMovementRange(ctx, factionIndex);
	
	// Validation
	if (!isValidMovement(ctx, factionIndex, fromTerritory, toTerritory, movementRange)) {
		std::cout << "    Movement FAILED: Invalid movement" << std::endl;
		return false;
	}
	
	// Check if player has enough units in source territory
	int unitsInSource = ctx.map.getUnitsInTerritory(fromTerritory, factionIndex);
	if (unitsInSource < totalUnits) {
		std::cout << "    Movement FAILED: Not enough units in " << fromTerritory
		          << " (need " << totalUnits << ", have " << unitsInSource << ")" << std::endl;
		return false;
	}
	
	// Execute movement
	ctx.map.removeUnitsFromTerritory(fromTerritory, factionIndex, normalUnits, eliteUnits);
	ctx.map.addUnitsToTerritory(toTerritory, factionIndex, normalUnits, eliteUnits);
	
	std::cout << "    Movement: Moved " << totalUnits << " units from " 
	          << fromTerritory << " to " << toTerritory << std::endl;
	
	return true;
}

std::vector<std::string> ShipAndMovePhase::getTerritoriesWithUnits(
	PhaseContext& ctx, int factionIndex) const {
	
	std::vector<std::string> territories;
	
	for (const auto& terr : ctx.map.getTerritories()) {
		int unitCount = ctx.map.getUnitsInTerritory(terr.name, factionIndex);
		if (unitCount > 0) {
			territories.push_back(terr.name);
		}
	}
	
	return territories;
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
		const territory* terr = ctx.map.getTerritory(terrName);
		if (terr != nullptr && terr->terrain == terrainType::city) {
			targetTerritory = terrName;
			break;
		}
	}
	
	if (targetTerritory.empty()) {
		targetTerritory = validTargets[0];
	}
	
	const territory* terr = ctx.map.getTerritory(targetTerritory);
	if (terr == nullptr) {
		return decision;
	}
	
	// Deploy up to 3 units, limited by spice and reserve
	int maxAffordable = player->getSpice() / calculateDeploymentCost(terr, 1);
	int unitsToDepl = std::min({3, maxAffordable, player->getUnitsReserve()});
	
	if (unitsToDepl <= 0) {
		return decision;
	}
	
	decision.shouldDeploy = true;
	decision.territoryName = targetTerritory;
	decision.normalUnits = unitsToDepl;
	decision.eliteUnits = 0;
	decision.spiceCost = calculateDeploymentCost(terr, unitsToDepl);
	
	return decision;
}

ShipAndMovePhase::MovementDecision ShipAndMovePhase::aiDecideMovement(
	PhaseContext& ctx, Player* player, int movementRange) const {
	
	MovementDecision decision;
	decision.shouldMove = false;
	decision.normalUnits = 0;
	decision.eliteUnits = 0;
	
	// Get territories with units
	std::vector<std::string> territoriesWithUnits = getTerritoriesWithUnits(ctx, player->getFactionIndex());
	
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
		const territory* terr = ctx.map.getTerritory(terrName);
		if (terr != nullptr && terr->spiceAmount > bestSpice) {
			bestSpice = terr->spiceAmount;
			bestTarget = terrName;
		}
	}
	
	// If no spice found, move to first reachable city
	if (bestTarget.empty()) {
		for (const auto& terrName : reachable) {
			const territory* terr = ctx.map.getTerritory(terrName);
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
	int unitsInSource = ctx.map.getUnitsInTerritory(sourceTerritory, player->getFactionIndex());
	int unitsToMove = std::max(1, unitsInSource / 2);
	
	decision.shouldMove = true;
	decision.fromTerritory = sourceTerritory;
	decision.toTerritory = bestTarget;
	decision.normalUnits = unitsToMove;
	decision.eliteUnits = 0;
	
	return decision;
}
