#include <phases/battle_phase.hpp>
#include <phases/phase_context.hpp>
#include <player.hpp>
#include <leader.hpp>
#include <map.hpp>
#include <iostream>
#include <algorithm>
#include <sstream>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

BattlePhase::BattlePhase() {
}

void BattlePhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("BATTLE Phase");
	}

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
				if (ctx.logger) {
					ctx.logger->logDebug(attacker->getFactionName() + "'s contested territories:");
				}
				for (size_t i = 0; i < contestedTerritories.size(); ++i) {
					if (ctx.logger) {
						ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + contestedTerritories[i]);
					}
				}
				if (ctx.logger) {
					ctx.logger->logDebug("Choose which to battle (1-" + std::to_string(contestedTerritories.size()) + "): ");
				}
				
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
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid choice. Try again: ");
					}
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

	if (ctx.logger) {
		if (totalBattles == 0) {
			ctx.logger->logDebug("No battles occurred this turn.");
		} else {
			ctx.logger->logDebug("=== " + std::to_string(totalBattles) + " battle(s) resolved ===");
		}
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

	if (ctx.logger) {
		std::string defendersStr;
		for (size_t i = 0; i < defenderIndices.size(); ++i) {
			defendersStr += view.players[defenderIndices[i]]->getFactionName();
			if (i < defenderIndices.size() - 1) defendersStr += ", ";
		}
		ctx.logger->logDebug("=== Battle in " + territoryName + " ===");
		ctx.logger->logDebug(attacker->getFactionName() + " (attacker) vs " + defendersStr);
	}

	// Get attacker's leaders
	const auto& aliveLeaders = attacker->getAliveLeaders();
	if (aliveLeaders.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug(attacker->getFactionName() + " has no alive leaders. Battle cannot occur.");
		}
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
	if (ctx.logger) {
		ctx.logger->logDebug(attacker->getFactionName() + " battle value: " + std::to_string(wheelValue) 
		          + " (wheel) + " + std::to_string(attackerLeader.power) + " (leader) = " + std::to_string(attackerValue));
	}

	// For now, pick first defender (in multi-defender scenarios, could be more complex)
	int defenderIdx = defenderIndices[0];
	Player* defender = view.players[defenderIdx];
	const auto& defenderLeaders = defender->getAliveLeaders();

	if (defenderLeaders.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug(defender->getFactionName() + " has no alive leaders. "
		          + attacker->getFactionName() + " wins by default.");
		}
		// Attacker loses wheelValue units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int defenderAllUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defenderAllUnits, 0);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				attacker->getFactionName() + " wins in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
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
	if (ctx.logger) {
		ctx.logger->logDebug(defender->getFactionName() + " battle value: " + std::to_string(defWheelValue) 
		          + " (wheel) + " + std::to_string(defenderLeader.power) + " (leader) = " + std::to_string(defenderValue));
	}

	// Determine winner (attacker wins ties)
	if (attackerValue > defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + attacker->getFactionName() + " wins!");
		}
		// Attacker loses wheelValue units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int allDefenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, allDefenderUnits, 0);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				attacker->getFactionName() + " defeats " + defender->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
	} else if (attackerValue < defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + defender->getFactionName() + " wins!");
		}
		// Defender wins: attacker loses all units, defender loses defWheelValue
		int allAttackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, allAttackerUnits, 0);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defWheelValue, 0);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				defender->getFactionName() + " defeats " + attacker->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = defender->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
	} else {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: Tie! " + attacker->getFactionName() + " wins (attacker advantage).");
		}
		// Attacker loses wheelValue, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, wheelValue, 0);
		int allDefenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, allDefenderUnits, 0);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"Tie in " + territoryName + ": " + attacker->getFactionName() + " wins",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
	}
}

int BattlePhase::getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	if (view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + ", enter battle wheel value (0-" + std::to_string(maxUnits) + "): ");
		}
		
		int choice = -1;
		while (choice < 0 || choice > maxUnits) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 0 || choice > maxUnits) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Must be 0-" + std::to_string(maxUnits) + ": ");
					}
					choice = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
				choice = -1;
			}
		}
		return choice;
	} else {
		// AI: random choice 0 to maxUnits
		int choice = rand() % (maxUnits + 1);
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " wheel value: " + std::to_string(choice));
		}
		return choice;
	}
}

int BattlePhase::selectLeaderForBattle(PhaseContext& ctx, int playerIndex, int aliveLeaderCount) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	if (view.interactiveMode && aliveLeaderCount > 1) {
		const auto& leaders = player->getAliveLeaders();
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + ", select leader for battle:");
		}
		for (int i = 0; i < (int)leaders.size(); ++i) {
			if (ctx.logger) {
				ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + leaders[i].name 
			          + " (power:" + std::to_string(leaders[i].power) + ")");
			}
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Choose (1-" + std::to_string(aliveLeaderCount) + "): ");
		}
		
		int choice = -1;
		while (choice < 1 || choice > aliveLeaderCount) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 1 || choice > aliveLeaderCount) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Try again: ");
					}
					choice = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
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
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " selects: " + leaders[bestIdx].name);
		}
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
