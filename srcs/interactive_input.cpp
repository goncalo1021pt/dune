#include "interactive_input.hpp"
#include "phases/phase_context.hpp"
#include "player.hpp"
#include "map.hpp"
#include <iostream>
#include <algorithm>

InteractiveInput::DeploymentChoice InteractiveInput::getDeploymentDecision(
	PhaseContext& ctx,
	Player* player,
	const std::vector<std::string>& validTargets) {

	displayGameState(ctx, player);
	
	// Territory selection loop - keep asking until valid or skip
	std::string territoryChoice;
	bool validTerritorySelected = false;
	std::string input;
	
	do {
		displayDeploymentMenu(validTargets, player);
		
		std::cin >> input;
		
		// Check for skip commands
		if (input == "0" || input == "skip" || input == "Skip") {
			std::cout << "  Shipment: Skipped" << std::endl;
			return {.territoryName = "", .normalUnits = 0, .eliteUnits = 0, .shouldDeploy = false};
		}
		
		// Try to parse as number first
		try {
			int index = std::stoi(input);
			if (index >= 1 && index <= static_cast<int>(validTargets.size())) {
				territoryChoice = validTargets[index - 1];  // Convert to 0-based index
				validTerritorySelected = true;
			} else if (index == 0) {
				std::cout << "  Shipment: Skipped" << std::endl;
				return {.territoryName = "", .normalUnits = 0, .eliteUnits = 0, .shouldDeploy = false};
			} else {
				std::cout << "  Invalid number (" << index << "). Please enter 1-" << validTargets.size() 
				          << " or '0/skip' to abort: ";
			}
		} catch (...) {
			// Not a number, treat as territory name
			auto it = std::find(validTargets.begin(), validTargets.end(), input);
			if (it != validTargets.end()) {
				territoryChoice = input;
				validTerritorySelected = true;
			} else {
				std::cout << "  Invalid territory '" << input << "'. Please try again or enter '0/skip': ";
			}
		}
	} while (!validTerritorySelected);

	std::cout << "  How many normal units to deploy? (0-" << player->getUnitsReserve() << "): ";
	int normalUnits = 0;
	std::cin >> normalUnits;
	normalUnits = std::max(0, std::min(normalUnits, player->getUnitsReserve()));

	int eliteUnits = 0;
	// Only ask for elite units if player has elite units available
	if (player->getEliteUnitsReserve() > 0) {
		std::cout << "  How many elite units to deploy? (0-" << player->getEliteUnitsReserve() << "): ";
		std::cin >> eliteUnits;
		eliteUnits = std::max(0, std::min(eliteUnits, player->getEliteUnitsReserve()));
	}

	return {
		.territoryName = territoryChoice,
		.normalUnits = normalUnits,
		.eliteUnits = eliteUnits,
		.shouldDeploy = (normalUnits + eliteUnits) > 0
	};
}

void InteractiveInput::displayGameState(PhaseContext& /* ctx */, Player* player) {
	std::cout << "\n  === Game State ===" << std::endl;
	std::cout << "  Player: " << player->getFactionName() << std::endl;
	std::cout << "  Spice: " << player->getSpice() << std::endl;
	std::cout << "  Units in reserve: " << player->getUnitsReserve() << std::endl;
	std::cout << "  Units deployed: " << player->getUnitsDeployed() << std::endl;
}

void InteractiveInput::displayDeploymentMenu(
	const std::vector<std::string>& validTargets,
	Player* /* player */) {
	std::cout << "\n  Valid deployment targets:" << std::endl;
	std::cout << "  0. Skip" << std::endl;
	for (size_t i = 0; i < validTargets.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << validTargets[i] << std::endl;
	}
	std::cout << "  Enter territory number (0 to skip) or name: ";
}

int InteractiveInput::showMenuAndGetChoice(const std::vector<std::string>& options) {
	for (size_t i = 0; i < options.size(); ++i) {
		std::cout << "  " << (i + 1) << ". " << options[i] << std::endl;
	}
	
	int choice = 0;
	std::cin >> choice;
	
	if (choice < 1 || choice > static_cast<int>(options.size())) {
		return -1;
	}
	
	return choice - 1;
}
