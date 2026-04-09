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

	// For now, pick first defender (in multi-defender scenarios, could be more complex)
	int defenderIdx = defenderIndices[0];
	Player* defender = view.players[defenderIdx];

	const std::string attackerFaction = attacker->getFactionName();
	const std::string defenderFaction = defender->getFactionName();
	const int attackerEliteStrength =
		(attackerFaction == "Emperor" && defenderFaction == "Fremen") ? 1 : 2;
	const int defenderEliteStrength =
		(defenderFaction == "Emperor" && attackerFaction == "Fremen") ? 1 : 2;
	
	// Mark leader as battled (no const_cast needed)
	attacker->markLeaderBattled(selectedLeaderIdx);
	const Leader& attackerLeader = aliveLeaders[selectedLeaderIdx];
	
	// Get attacker's wheel value (units to commit/lose)
	int attackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
	int wheelValue = getBattleWheelChoice(ctx, attackerIdx, attackerUnits, territoryName, attackerEliteStrength);
	
	// Calculate attacker's actual unit strength (elite units count as 2x)
	auto [normalUnits, eliteUnits] = view.map.getUnitBreakdown(territoryName, attackerIdx);
	int unitStrength = calculateUnitStrength(wheelValue, normalUnits, eliteUnits, attackerEliteStrength);

	// Calculate attacker's battle value
	int attackerValue = unitStrength + attackerLeader.power;
	if (ctx.logger) {
		ctx.logger->logDebug(attacker->getFactionName() + " battle value: " + std::to_string(unitStrength) 
		          + " (units: " + std::to_string(wheelValue) + " committed from " + std::to_string(normalUnits) 
		          + "N+" + std::to_string(eliteUnits) + "E) + " + std::to_string(attackerLeader.power) 
		          + " (leader) = " + std::to_string(attackerValue));
	}

	const auto& defenderLeaders = defender->getAliveLeaders();

	if (defenderLeaders.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug(defender->getFactionName() + " has no alive leaders. "
		          + attacker->getFactionName() + " wins by default.");
		}
		
		// Attacker survives with remaining strength after dialing
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses dialed strength worth of units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		
		// Get defender's breakdown for full removal
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
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
	int defWheelValue = getBattleWheelChoice(ctx, defenderIdx, defenderUnits, territoryName, defenderEliteStrength);
	
	// Calculate defender's actual unit strength (elite units count as 2x)
	auto [defNormalUnits, defEliteUnits] = view.map.getUnitBreakdown(territoryName, defenderIdx);
	int defUnitStrength = calculateUnitStrength(defWheelValue, defNormalUnits, defEliteUnits, defenderEliteStrength);

	int defenderValue = defUnitStrength + defenderLeader.power;
	if (ctx.logger) {
		ctx.logger->logDebug(defender->getFactionName() + " battle value: " + std::to_string(defUnitStrength) 
		          + " (units: " + std::to_string(defWheelValue) + " committed from " + std::to_string(defNormalUnits) 
		          + "N+" + std::to_string(defEliteUnits) + "E) + " + std::to_string(defenderLeader.power) 
		          + " (leader) = " + std::to_string(defenderValue));
	}

	// Determine winner (attacker wins ties)
	if (attackerValue > defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + attacker->getFactionName() + " wins!");
		}
		
		// Winner (attacker) keeps totalStrength - dialedStrength
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses casualties, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				attacker->getFactionName() + " defeats " + defender->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger faction ability hooks for battle win
		attacker->getFactionAbility()->onBattleWon(ctx, defenderIdx);
		
	} else if (attackerValue < defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + defender->getFactionName() + " wins!");
		}
		
		// Calculate unit breakdowns for removal
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalDefenderStrength = calculateUnitStrength(defenderUnits, defNormalUnits, defEliteUnits, defenderEliteStrength);
		int defenderRemainingStrength = std::max(0, totalDefenderStrength - defUnitStrength);
		auto [defNKilled, defEKilled] = askCasualtyDistribution(ctx, defenderIdx, defenderRemainingStrength, defN, defE, defenderEliteStrength);
		
		// Defender wins: attacker loses all units, defender loses casualties
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aN, aE);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defNKilled, defEKilled);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				defender->getFactionName() + " defeats " + attacker->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = defender->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger faction ability hooks for battle win
		defender->getFactionAbility()->onBattleWon(ctx, attackerIdx);
		
	} else {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: Tie! " + attacker->getFactionName() + " wins (attacker advantage).");
		}
		
		// Winner (attacker in tie) keeps totalStrength - dialedStrength
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses casualties, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"Tie in " + territoryName + ": " + attacker->getFactionName() + " wins",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger faction ability hooks for battle win (attacker wins tie)
		attacker->getFactionAbility()->onBattleWon(ctx, defenderIdx);
	}
}

// Helper: Calculate actual unit strength based on commitment and unit type mix
// Elite units count as 2x strength, normal units as 1x
// When committing X units from (N normal, E elite), commit elite first then normal
int BattlePhase::calculateUnitStrength(int commitUnits, int normalAvailable, int eliteAvailable, int eliteStrength) {
	// Commit elite units first (they're worth more)
	int eliteCommitted = std::min(commitUnits, eliteAvailable);
	int normalCommitted = std::min(commitUnits - eliteCommitted, normalAvailable);
	
	// Strength = normal*1 + elite*eliteStrength
	return normalCommitted + (eliteCommitted * eliteStrength);
}

// Helper: Convert desired strength to unit count
// Elite units count as 2x, normal as 1x. Commits elite first (greedy).
int BattlePhase::convertStrengthToUnitCount(int desiredStrength, int normalAvailable, int eliteAvailable, int eliteStrength) {
	if (eliteStrength <= 1) {
		return std::min(desiredStrength, normalAvailable + eliteAvailable);
	}

	// Commit elite units first to reach strength
	int eliteToCommit = std::min(desiredStrength / eliteStrength, eliteAvailable);
	int strengthFromElite = eliteToCommit * eliteStrength;
	int remainingStrength = desiredStrength - strengthFromElite;
	
	// Fill remaining with normal units
	int normalToCommit = std::min(remainingStrength, normalAvailable);
	
	// Total units committed
	return eliteToCommit + normalToCommit;
}

int BattlePhase::getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits,
									  const std::string& territoryName, int eliteStrength) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	// Calculate max strength available
	int maxStrength = 0;
	int normalAvailable = 0, eliteAvailable = 0;
	if (!territoryName.empty()) {
		auto [n, e] = view.map.getUnitBreakdown(territoryName, playerIndex);
		normalAvailable = n;
		eliteAvailable = e;
		maxStrength = calculateUnitStrength(maxUnits, n, e, eliteStrength);
	}
	
	if (view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + ", enter battle strength (0-" + std::to_string(maxStrength) + "): ");
		}
		
		int choice = -1;
		while (choice < 0 || choice > maxStrength) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 0 || choice > maxStrength) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Must be 0-" + std::to_string(maxStrength) + ": ");
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
		
		// Convert strength to unit count for removal calculation
		return convertStrengthToUnitCount(choice, normalAvailable, eliteAvailable, eliteStrength);
	} else {
		// AI: random strength choice 0 to maxStrength
		int strength = rand() % (maxStrength + 1);
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " battle strength: " + std::to_string(strength));
		}
		return convertStrengthToUnitCount(strength, normalAvailable, eliteAvailable, eliteStrength);
	}
}

// Ask how many elite units survive based on remaining strength.
// Returns casualties as (normalKilled, eliteKilled).
std::pair<int, int> BattlePhase::askCasualtyDistribution(PhaseContext& ctx, int playerIndex, int remainingStrength,
	                                                     int normalAvailable, int eliteAvailable, int eliteStrength) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	int totalStrength = normalAvailable + (eliteAvailable * eliteStrength);

	// No survivors -> everyone dies.
	if (remainingStrength <= 0) {
		return {normalAvailable, eliteAvailable};
	}

	// All survive.
	if (remainingStrength >= totalStrength) {
		return {0, 0};
	}

	// Feasible elite survivors satisfy:
	// eliteSurvive*eliteStrength + normalSurvive = remainingStrength
	// with 0<=eliteSurvive<=eliteAvailable and 0<=normalSurvive<=normalAvailable.
	int minEliteSurvive = std::max(0, (remainingStrength - normalAvailable + eliteStrength - 1) / eliteStrength);
	int maxEliteSurvive = std::min(eliteAvailable, remainingStrength / eliteStrength);

	// Clamp to a safe feasible value if arithmetic corner-case happens.
	if (minEliteSurvive > maxEliteSurvive) {
		int eliteSurvive = std::max(0, std::min(eliteAvailable, remainingStrength / eliteStrength));
		int normalSurvive = std::max(0, std::min(normalAvailable, remainingStrength - (eliteSurvive * eliteStrength)));
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	}

	// Deterministic case, no need to ask.
	if (minEliteSurvive == maxEliteSurvive) {
		int eliteSurvive = minEliteSurvive;
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	}
	
	if (view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " keeps " + std::to_string(remainingStrength) + " strength after battle.");
			ctx.logger->logDebug("How many elite units survive? (" + std::to_string(minEliteSurvive) + "-" + std::to_string(maxEliteSurvive) + "): ");
		}
		
		int eliteSurvive = -1;
		while (eliteSurvive < minEliteSurvive || eliteSurvive > maxEliteSurvive) {
			std::string input;
			std::getline(std::cin, input);
			try {
				eliteSurvive = std::stoi(input);
				if (eliteSurvive < minEliteSurvive || eliteSurvive > maxEliteSurvive) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Must be " + std::to_string(minEliteSurvive) + "-" + std::to_string(maxEliteSurvive) + ": ");
					}
					eliteSurvive = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
				eliteSurvive = -1;
			}
		}

		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		int normalKilled = normalAvailable - normalSurvive;
		int eliteKilled = eliteAvailable - eliteSurvive;
		return {normalKilled, eliteKilled};
	} else {
		// AI: keep as many elite as possible among feasible survivor choices.
		int eliteSurvive = maxEliteSurvive;
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		int normalKilled = normalAvailable - normalSurvive;
		int eliteKilled = eliteAvailable - eliteSurvive;
		return {normalKilled, eliteKilled};
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

