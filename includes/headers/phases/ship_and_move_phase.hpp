#pragma once

#include "phase.hpp"
#include <vector>
#include <string>

// Forward declaration
struct territory;
class InteractiveInput;

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
	 * Uses faction ability to determine cost
	 */
	int calculateDeploymentCost(const territory* terr, int unitCount, Player* player) const;
	
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
	
	int calculateMovementRange(PhaseContext& ctx, int factionIndex) const;
	
	std::vector<std::string> getReachableTerritories(
		PhaseContext& ctx, 
		const std::string& sourceTerritoryName,
		int movementRange,
		int factionIndex
	) const;
	
	bool moveUnits(PhaseContext& ctx, int factionIndex,
	               const std::string& fromTerritory, 
	               const std::string& toTerritory,
	               int normalUnits, int eliteUnits);
	
	// === AI DECISION METHODS (Placeholder) ===

	struct DeploymentDecision {
		std::string territoryName;
		int normalUnits;
		int eliteUnits;
		int spiceCost;
		bool shouldDeploy;
	};
	
	DeploymentDecision aiDecideDeployment(PhaseContext& ctx, Player* player) const;
	
	struct MovementDecision {
		std::string fromTerritory;
		std::string toTerritory;
		int normalUnits;
		int eliteUnits;
		bool shouldMove;
	};
	
	MovementDecision aiDecideMovement(PhaseContext& ctx, Player* player, int movementRange) const;
	
	// === VALIDATION METHODS ===
	
	bool isValidDeployment(PhaseContext& ctx, int factionIndex, 
	                       const std::string& territoryName, int unitCount) const;
	
	bool isValidMovement(PhaseContext& ctx, int factionIndex,
	                     const std::string& fromTerritory, 
	                     const std::string& toTerritory,
	                     int movementRange) const;
};
