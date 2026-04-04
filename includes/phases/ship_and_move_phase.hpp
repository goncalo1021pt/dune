#pragma once

#include "phase.hpp"
#include <vector>
#include <string>

// Forward declaration
struct territory;

/**
 * ShipAndMovePhase: Handles player deployments and movements
 * 
 * Phase order:
 * 1. Each player in turn order performs:
 *    a) Optional shipment (deploy units from reserve)
 *    b) Optional movement (move deployed units)
 * 
 * Shipment costs:
 * - City territories: 1 spice per unit
 * - Non-city territories: 2 spice per unit
 * 
 * Movement range:
 * - Base: 1 territory
 * - Special: 3 territories if controlling Arrakeen OR Carthag
 */
class ShipAndMovePhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	// === SHIPMENT METHODS ===
	
	/**
	 * Execute shipment phase for a single player
	 * Returns true if player performed any shipment action
	 */
	bool executePlayerShipment(PhaseContext& ctx, Player* player);
	
	/**
	 * Calculate spice cost for deploying units to a territory
	 */
	int calculateDeploymentCost(const territory* terr, int unitCount) const;
	
	/**
	 * Get list of valid territories where player can deploy
	 */
	std::vector<std::string> getValidDeploymentTargets(PhaseContext& ctx, int factionIndex) const;
	
	/**
	 * Deploy units to a specific territory
	 * Returns true if deployment was successful
	 */
	bool deployUnits(PhaseContext& ctx, Player* player, 
	                 const std::string& territoryName, int normalUnits, int eliteUnits);
	
	// === MOVEMENT METHODS ===
	
	/**
	 * Execute movement phase for a single player
	 * Returns true if player performed any movement action
	 */
	bool executePlayerMovement(PhaseContext& ctx, Player* player);
	
	/**
	 * Calculate maximum movement range for a faction
	 * - Returns 3 if controlling Arrakeen or Carthag (special movement)
	 * - Returns 1 otherwise (base movement)
	 */
	int calculateMovementRange(PhaseContext& ctx, int factionIndex) const;
	
	/**
	 * Get all territories reachable from source within movement range
	 * Uses BFS to find all territories within N steps
	 */
	std::vector<std::string> getReachableTerritories(
		PhaseContext& ctx, 
		const std::string& sourceTerritoryName,
		int movementRange,
		int factionIndex
	) const;
	
	/**
	 * Move units from one territory to another
	 * Returns true if movement was successful
	 */
	bool moveUnits(PhaseContext& ctx, int factionIndex,
	               const std::string& fromTerritory, 
	               const std::string& toTerritory,
	               int normalUnits, int eliteUnits);
	
	/**
	 * Get list of territories where faction has units
	 */
	std::vector<std::string> getTerritoriesWithUnits(PhaseContext& ctx, int factionIndex) const;
	
	// === AI DECISION METHODS (Placeholder) ===
	
	/**
	 * AI decides which territory to deploy to and how many units
	 * Returns: {territoryName, normalUnits, eliteUnits, spiceCost}
	 */
	struct DeploymentDecision {
		std::string territoryName;
		int normalUnits;
		int eliteUnits;
		int spiceCost;
		bool shouldDeploy;
	};
	
	DeploymentDecision aiDecideDeployment(PhaseContext& ctx, Player* player) const;
	
	/**
	 * AI decides which units to move and where
	 * Returns: {fromTerritory, toTerritory, normalUnits, eliteUnits}
	 */
	struct MovementDecision {
		std::string fromTerritory;
		std::string toTerritory;
		int normalUnits;
		int eliteUnits;
		bool shouldMove;
	};
	
	MovementDecision aiDecideMovement(PhaseContext& ctx, Player* player, int movementRange) const;
	
	// === VALIDATION METHODS ===
	
	/**
	 * Check if deployment is valid
	 */
	bool isValidDeployment(PhaseContext& ctx, int factionIndex, 
	                       const std::string& territoryName, int unitCount) const;
	
	/**
	 * Check if movement is valid
	 */
	bool isValidMovement(PhaseContext& ctx, int factionIndex,
	                     const std::string& fromTerritory, 
	                     const std::string& toTerritory,
	                     int movementRange) const;
};
