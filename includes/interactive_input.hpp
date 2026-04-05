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
		bool shouldDeploy;
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
