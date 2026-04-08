#pragma once

#include "phase.hpp"
#include <vector>
#include <string>

struct territory;
class InteractiveInput;

class ShipAndMovePhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	// === SHIPMENT ===

	bool executePlayerShipment(PhaseContext& ctx, Player* player);

	int calculateDeploymentCost(const territory* terr, int unitCount, Player* player) const;

	std::vector<std::string> getValidDeploymentTargets(PhaseContext& ctx, int factionIndex) const;

	// sector: which sector of the territory to place units in.
	//         Pass -1 to auto-select the first safe sector.
	bool deployUnits(PhaseContext& ctx, Player* player,
	                 const std::string& territoryName,
	                 int normalUnits, int eliteUnits,
	                 int sector);

	// === MOVEMENT ===

	bool executePlayerMovement(PhaseContext& ctx, Player* player);

	int calculateMovementRange(PhaseContext& ctx, int factionIndex) const;

	// sourceUnitSector: the sector within sourceTerritoryName where the moving
	// units currently sit. The storm check uses this to decide if they can leave.
	std::vector<std::string> getReachableTerritories(
		PhaseContext& ctx,
		const std::string& sourceTerritoryName,
		int sourceUnitSector,
		int movementRange,
		int factionIndex) const;

	// fromSector / toSector: specific sectors for the move.
	bool moveUnits(PhaseContext& ctx, int factionIndex,
	               const std::string& fromTerritory, int fromSector,
	               const std::string& toTerritory,   int toSector,
	               int normalUnits, int eliteUnits);

	// === AI DECISIONS ===

	struct DeploymentDecision {
		std::string territoryName;
		int normalUnits;
		int eliteUnits;
		int sector;      // -1 = auto
		int spiceCost;
		bool shouldDeploy;
	};
	
	DeploymentDecision aiDecideDeployment(PhaseContext& ctx, Player* player) const;
	
	struct MovementDecision {
		std::string fromTerritory;
		int fromSector;   // -1 = auto
		std::string toTerritory;
		int toSector;     // -1 = auto
		int normalUnits;
		int eliteUnits;
		bool shouldMove;
	};
	
	MovementDecision aiDecideMovement(PhaseContext& ctx, Player* player, int movementRange) const;
	
	// === VALIDATION ===
	
	bool isValidDeployment(PhaseContext& ctx, int factionIndex,
	                       const std::string& territoryName, int unitCount) const;
	
	bool isValidMovement(PhaseContext& ctx, int factionIndex,
	                     const std::string& fromTerritory,
	                     int fromSector,
	                     const std::string& toTerritory,
	                     int movementRange) const;
};