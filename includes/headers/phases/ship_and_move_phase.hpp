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

	// Guild special shipment types
	bool executeGuildCrossShipment(PhaseContext& ctx, Player* player);
	bool executeGuildReturnToReserveShipment(PhaseContext& ctx, Player* player);

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

	// Drive an interactive deployment via a sequence of primitive adapter
	// requests (select territory, int normal, int elite, select sector).
	// Mirrors the engine-side option computation that aiDecideDeployment
	// uses. Returns shouldDeploy=false if the adapter is null, the player
	// declines via empty territory select, or any sub-decision fails.
	DeploymentDecision interactiveDeploymentDecision(
		PhaseContext& ctx, Player* player,
		const std::vector<std::string>& validTargets) const;
	
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

	// Drive an interactive movement via primitive adapter requests:
	// select source territory, select source sector (auto if only one
	// movable), select destination territory (BFS-reachable), int normal,
	// int elite, select destination sector. Returns shouldMove=false if
	// the adapter is null, the player declines, or any sub-decision fails.
	MovementDecision interactiveMovementDecision(
		PhaseContext& ctx, Player* player,
		const std::vector<std::string>& territoriesWithUnits,
		int movementRange) const;
	
	// === VALIDATION ===
	
	bool isValidDeployment(PhaseContext& ctx, int factionIndex,
	                       const std::string& territoryName, int unitCount) const;
	
	bool isValidMovement(PhaseContext& ctx, int factionIndex,
	                     const std::string& fromTerritory,
	                     int fromSector,
	                     const std::string& toTerritory,
	                     int movementRange) const;
};