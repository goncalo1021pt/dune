#pragma once

#include <string>
#include <vector>

class PhaseContext;
class Player;

/**
 * Interactive input handler for testing player decisions
 * Provides console menus and command parsing for manual game control
 */
class InteractiveInput {
public:
	struct DeploymentChoice {
		std::string territoryName;
		int normalUnits;
		int eliteUnits;
		int sector;       // -1 = auto, otherwise specific sector
		bool shouldDeploy;
	};

	struct MovementChoice {
		std::string fromTerritory;
		std::string toTerritory;
		int normalUnits;
		int eliteUnits;
		int fromSector;  // -1 = auto, otherwise specific sector
		int toSector;    // -1 = auto, otherwise specific sector
		bool shouldMove;
	};

	/**
	 * Present deployment options to player and get their choice
	 */
	static DeploymentChoice getDeploymentDecision(
		PhaseContext& ctx, 
		Player* player,
		const std::vector<std::string>& validTargets
	);

	/**
	 * Present movement options to player and get their choice
	 */
	static MovementChoice getMovementDecision(
		PhaseContext& ctx,
		Player* player,
		const std::vector<std::string>& territoriesWithUnits,
		int movementRange
	);

	/**
	 * Display menu and parse user input
	 */
	static int showMenuAndGetChoice(const std::vector<std::string>& options);

private:
	static void displayGameState(PhaseContext& ctx, Player* player);
	static void displayDeploymentMenu(
		const std::vector<std::string>& validTargets,
		Player* player
	);
};
