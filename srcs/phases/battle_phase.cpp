#include <phases/battle_phase.hpp>
#include <phases/phase_context.hpp>
#include <player.hpp>
#include <leader.hpp>
#include <map.hpp>
#include <iostream>
#include <algorithm>
#include <sstream>

BattlePhase::BattlePhase() {
}

void BattlePhase::execute(PhaseContext& ctx) {
	std::cout << "  BATTLE Phase" << std::endl;

	// Get view for this phase
	auto view = ctx.getBattleView();

	// Battle resolution follows turn order
	// Each player's turn: resolve all their contested battles
	const auto& turnOrder = view.turnOrder;
	int totalBattles = 0;

	for (int playerIdx : turnOrder) {
		Player* attacker = view.players[playerIdx];
		
		// Find all contested territories where this player has units
		std::vector<std::string> contestedTerritories;
		const auto& territories = view.map.getTerritories();
		
		for (const auto& terr : territories) {
			// Skip Polar Sink (no battles)
			if (terr.name == "Polar Sink") continue;
			
			// Check if this player has units here
			int playerUnits = view.map.getUnitsInTerritory(terr.name, playerIdx);
			if (playerUnits == 0) continue;
			
			// Check if contested (other players also have units)
			bool contested = false;
			for (size_t i = 0; i < view.players.size(); ++i) {
				if (i != (size_t)playerIdx) {
					int otherUnits = view.map.getUnitsInTerritory(terr.name, i);
					if (otherUnits > 0) {
						contested = true;
						break;
					}
				}
			}
			
			if (contested) {
				contestedTerritories.push_back(terr.name);
			}
		}

		// Player resolves battles in contested territories
		while (!contestedTerritories.empty()) {
			if (view.interactiveMode && contestedTerritories.size() > 1) {
				// Ask player which battle to resolve first
				std::cout << "\n    " << attacker->getFactionName() << "'s contested territories:" << std::endl;
				for (size_t i = 0; i < contestedTerritories.size(); ++i) {
					std::cout << "      " << (i + 1) << ". " << contestedTerritories[i] << std::endl;
				}
				std::cout << "    Choose which to battle (1-" << contestedTerritories.size() << "): ";
				
				int choice = 0;
				std::string input;
				while (true) {
					std::getline(std::cin, input);
					try {
						choice = std::stoi(input);
						if (choice >= 1 && choice <= (int)contestedTerritories.size()) {
							break;
						}
					} catch (...) {}
					std::cout << "    Invalid choice. Try again: ";
				}
				choice--; // Convert to 0-indexed
				
				// Resolve this battle
				resolveBattle(ctx, playerIdx, contestedTerritories[choice]);
				totalBattles++;
				
				// Remove from list
				contestedTerritories.erase(contestedTerritories.begin() + choice);
			} else if (!contestedTerritories.empty()) {
				// Only one contested territory or non-interactive mode
				resolveBattle(ctx, playerIdx, contestedTerritories[0]);
				totalBattles++;
				contestedTerritories.erase(contestedTerritories.begin());
			}
		}
	}

	if (totalBattles == 0) {
		std::cout << "    No battles occurred this turn." << std::endl;
	} else {
		std::cout << "\n    === " << totalBattles << " battle(s) resolved ===" << std::endl;
	}
}

void BattlePhase::resolveBattle(PhaseContext& ctx, int attackerIdx, const std::string& territoryName) {
	auto view = ctx.getBattleView();
	Player* attacker = view.players[attackerIdx];
	
	// Find all defenders in this territory
	std::vector<int> defenderIndices;
	for (size_t i = 0; i < view.players.size(); ++i) {
		if (i != (size_t)attackerIdx) {
			int units = view.map.getUnitsInTerritory(territoryName, i);
			if (units > 0) {
				defenderIndices.push_back(i);
			}
		}
	}

	if (defenderIndices.empty()) return; // No defenders

	std::cout << "\n    === Battle in " << territoryName << " ===" << std::endl;
	std::cout << "    " << attacker->getFactionName() << " (attacker) vs ";
	for (size_t i = 0; i < defenderIndices.size(); ++i) {
		std::cout << view.players[defenderIndices[i]]->getFactionName();
		if (i < defenderIndices.size() - 1) std::cout << ", ";
	}
	std::cout << std::endl;

	// Get attacker's leaders
	const auto& aliveLeaders = attacker->getAliveLeaders();
	if (aliveLeaders.empty()) {
		std::cout << "    " << attacker->getFactionName() << " has no alive leaders. Battle cannot occur." << std::endl;
		return;
	}

	int selectedLeaderIdx = selectLeaderForBattle(ctx, attackerIdx, aliveLeaders.size());
	if (selectedLeaderIdx < 0) return;
	
	// Mark leader as battled (no const_cast needed)
	attacker->markLeaderBattled(selectedLeaderIdx);
	const Leader& attackerLeader = aliveLeaders[selectedLeaderIdx];
	
	// Get attacker's wheel value (units to commit/lose)
	int attackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
	int wheelValue = getBattleWheelChoice(ctx, attackerIdx, attackerUnits);

	// Calculate attacker's battle value
	int attackerValue = wheelValue + attackerLeader.power;
	std::cout << "    " << attacker->getFactionName() << " battle value: " << wheelValue 
	          << " (wheel) + " << attackerLeader.power << " (leader) = " << attackerValue << std::endl;

	// For now, pick first defender (in multi-defender scenarios, could be more complex)
	int defenderIdx = defenderIndices[0];
	Player* defender = view.players[defenderIdx];
	const auto& defenderLeaders = defender->getAliveLeaders();

	if (defenderLeaders.empty()) {
		std::cout << "    " << defender->getFactionName() << " has no alive leaders. "
		          << attacker->getFactionName() << " wins by default." << std::endl;
		// Attacker loses wheelValue units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int defenderAllUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defenderAllUnits, 0);
		return;
	}

	// Get defender's leader
	int defSelectedLeaderIdx = selectLeaderForBattle(ctx, defenderIdx, defenderLeaders.size());
	if (defSelectedLeaderIdx < 0) return;
	
	// Mark defender leader as battled (no const_cast needed)
	defender->markLeaderBattled(defSelectedLeaderIdx);
	const Leader& defenderLeader = defenderLeaders[defSelectedLeaderIdx];
	
	// Get defender's wheel value
	int defenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
	int defWheelValue = getBattleWheelChoice(ctx, defenderIdx, defenderUnits);

	int defenderValue = defWheelValue + defenderLeader.power;
	std::cout << "    " << defender->getFactionName() << " battle value: " << defWheelValue 
	          << " (wheel) + " << defenderLeader.power << " (leader) = " << defenderValue << std::endl;

	// Determine winner (attacker wins ties)
	std::cout << "    Result: ";
	if (attackerValue > defenderValue) {
		std::cout << attacker->getFactionName() << " wins!" << std::endl;
		// Attacker loses wheelValue units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int allDefenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, allDefenderUnits, 0);
	} else if (attackerValue < defenderValue) {
		std::cout << defender->getFactionName() << " wins!" << std::endl;
		// Defender wins: attacker loses all units, defender loses defWheelValue
		int allAttackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, allAttackerUnits, 0);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defWheelValue, 0);
	} else {
		std::cout << "Tie! " << attacker->getFactionName() << " wins (attacker advantage)." << std::endl;
		// Attacker loses wheelValue, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int allDefenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, allDefenderUnits, 0);
	}
}

int BattlePhase::getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	if (view.interactiveMode) {
		std::cout << "    " << player->getFactionName() << ", enter battle wheel value (0-" << maxUnits << "): ";
		
		int choice = -1;
		while (choice < 0 || choice > maxUnits) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 0 || choice > maxUnits) {
					std::cout << "    Invalid. Must be 0-" << maxUnits << ": ";
					choice = -1;
				}
			} catch (...) {
				std::cout << "    Invalid input. Try again: ";
				choice = -1;
			}
		}
		return choice;
	} else {
		// AI: random choice 0 to maxUnits
		int choice = rand() % (maxUnits + 1);
		std::cout << "    " << player->getFactionName() << " wheel value: " << choice << std::endl;
		return choice;
	}
}

int BattlePhase::selectLeaderForBattle(PhaseContext& ctx, int playerIndex, int aliveLeaderCount) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	if (view.interactiveMode && aliveLeaderCount > 1) {
		const auto& leaders = player->getAliveLeaders();
		std::cout << "    " << player->getFactionName() << ", select leader for battle:" << std::endl;
		for (int i = 0; i < (int)leaders.size(); ++i) {
			std::cout << "      " << (i + 1) << ". " << leaders[i].name 
			          << " (power:" << leaders[i].power << ")" << std::endl;
		}
		std::cout << "    Choose (1-" << aliveLeaderCount << "): ";
		
		int choice = -1;
		while (choice < 1 || choice > aliveLeaderCount) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 1 || choice > aliveLeaderCount) {
					std::cout << "    Invalid. Try again: ";
					choice = -1;
				}
			} catch (...) {
				std::cout << "    Invalid input. Try again: ";
				choice = -1;
			}
		}
		return choice - 1; // Convert to 0-indexed
	} else {
		// Single leader or AI mode - pick highest power
		const auto& leaders = player->getAliveLeaders();
		int bestIdx = 0;
		int bestPower = -1;
		for (int i = 0; i < (int)leaders.size(); ++i) {
			if (leaders[i].power > bestPower) {
				bestPower = leaders[i].power;
				bestIdx = i;
			}
		}
		std::cout << "    " << player->getFactionName() << " selects: " << leaders[bestIdx].name << std::endl;
		return bestIdx;
	}
}

std::vector<int> BattlePhase::getPlayersInTerritory(PhaseContext& ctx, const territory& territory) {
	auto view = ctx.getBattleView();
	std::vector<int> players;

	for (size_t i = 0; i < view.players.size(); ++i) {
		// Check if this player has units in the territory
		int unitsPresent = view.map.getUnitsInTerritory(territory.name, i);
		if (unitsPresent > 0) {
			players.push_back(i);
		}
	}

	return players;
}
