#include "phases/ship_and_move_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include "player.hpp"
#include "interactive_input.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <set>
#include "cards/spice_deck.hpp"
#include "factions/bene_gesserit_ability.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"

namespace {

struct GuildSourceSelection {
	std::string territory;
	int sector;
	int normalAvailable;
	int eliteAvailable;
};

bool readIntInRange(PhaseContext& ctx, int minValue, int maxValue, int& outValue) {
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		try {
			int value = std::stoi(input);
			if (value >= minValue && value <= maxValue) {
				outValue = value;
				return true;
			}
		} catch (...) {
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Invalid choice. Try again: ");
		}
	}
}

int promptMenuChoice(PhaseContext& ctx, const std::string& title, const std::vector<std::string>& options) {
	if (ctx.logger) {
		ctx.logger->logDebug(title);
		for (size_t i = 0; i < options.size(); ++i) {
			ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + options[i]);
		}
	}

	int choice = -1;
	if (!readIntInRange(ctx, 0, static_cast<int>(options.size()), choice)) {
		return -1;
	}
	if (choice == 0) {
		return -1;
	}
	return choice - 1;
}

bool selectGuildSource(PhaseContext& ctx, int factionIndex, const std::string& failPrefix, GuildSourceSelection& outSel) {
	auto view = ctx.getShipAndMoveView();

	std::vector<std::string> sourceTerritories;
	for (const auto& name : view.map.getTerritoriesWithUnits(factionIndex)) {
		const territory* terr = view.map.getTerritory(name);
		if (!terr) continue;
		bool hasSafeSource = false;
		for (int s : terr->sectors) {
			if (GameMap::canLeaveSector(s, ctx.stormSector) &&
				view.map.getUnitsInTerritorySector(name, factionIndex, s) > 0) {
				hasSafeSource = true;
				break;
			}
		}
		if (hasSafeSource) sourceTerritories.push_back(name);
	}

	if (sourceTerritories.empty()) {
		if (ctx.logger) ctx.logger->logDebug(failPrefix + " FAILED: no eligible source territory");
		return false;
	}

	int sourceIdx = promptMenuChoice(ctx, failPrefix + ": choose source territory (0 to cancel)", sourceTerritories);
	if (sourceIdx < 0) return false;
	std::string fromTerritory = sourceTerritories[sourceIdx];

	const territory* src = view.map.getTerritory(fromTerritory);
	if (!src) return false;

	std::vector<int> sourceSectors;
	for (int s : src->sectors) {
		if (GameMap::canLeaveSector(s, ctx.stormSector) &&
			view.map.getUnitsInTerritorySector(fromTerritory, factionIndex, s) > 0) {
			sourceSectors.push_back(s);
		}
	}
	if (sourceSectors.empty()) {
		if (ctx.logger) ctx.logger->logDebug(failPrefix + " FAILED: no source sector can leave storm");
		return false;
	}

	int fromSector = sourceSectors[0];
	if (sourceSectors.size() > 1) {
		std::vector<std::string> sectorOptions;
		for (int s : sourceSectors) {
			int units = view.map.getUnitsInTerritorySector(fromTerritory, factionIndex, s);
			sectorOptions.push_back("Sector " + std::to_string(s) + " (" + std::to_string(units) + " units)");
		}
		int sectorIdx = promptMenuChoice(ctx, "Choose source sector (0 to cancel):", sectorOptions);
		if (sectorIdx < 0) return false;
		fromSector = sourceSectors[sectorIdx];
	}

	int unitsInSource = view.map.getUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	int eliteInSource = view.map.getEliteUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	outSel.territory = fromTerritory;
	outSel.sector = fromSector;
	outSel.eliteAvailable = eliteInSource;
	outSel.normalAvailable = unitsInSource - eliteInSource;
	return true;
}

bool selectUnitSplitFromSource(PhaseContext& ctx,
	int normalAvailable,
	int eliteAvailable,
	const std::string& totalPrompt,
	int& normalUnits,
	int& eliteUnits) {
	const int totalAvailable = normalAvailable + eliteAvailable;
	if (ctx.logger) {
		ctx.logger->logDebug("Units in source: " + std::to_string(normalAvailable) + " normal, " +
			std::to_string(eliteAvailable) + " elite");
		ctx.logger->logDebug(totalPrompt + " (0-" + std::to_string(totalAvailable) + "): ");
	}

	int totalUnits = 0;
	if (!readIntInRange(ctx, 0, totalAvailable, totalUnits)) {
		return false;
	}
	if (totalUnits == 0) {
		return false;
	}

	eliteUnits = 0;
	if (eliteAvailable > 0) {
		int maxElite = std::min(totalUnits, eliteAvailable);
		int minElite = std::max(0, totalUnits - normalAvailable);
		if (ctx.logger) {
			ctx.logger->logDebug("How many elite among these " + std::to_string(totalUnits) + "? (" +
				std::to_string(minElite) + "-" + std::to_string(maxElite) + "): ");
		}
		if (!readIntInRange(ctx, minElite, maxElite, eliteUnits)) {
			return false;
		}
	}

	normalUnits = totalUnits - eliteUnits;
	return true;
}

} // namespace

// ============================================================================
// MAIN PHASE EXECUTION
// ============================================================================

void ShipAndMovePhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("SHIP_AND_MOVE Phase");
	}

	auto view = ctx.getShipAndMoveView();
	std::vector<bool> didAny(view.players.size(), false);

	int beneIndex = -1;
	BeneGesseritAbility* beneAbility = nullptr;
	if (ctx.featureSettings.advancedFactionAbilities) {
		for (size_t i = 0; i < ctx.players.size(); ++i) {
			auto* bg = dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(static_cast<int>(i)));
			if (bg) {
				beneIndex = static_cast<int>(i);
				beneAbility = bg;
				beneAbility->beginTurn(ctx.turnNumber);
				break;
			}
		}
	}

	if (beneAbility && ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] Pre-shipment advisor battle declaration window");
	}
	if (beneAbility) {
		for (const auto& terr : view.map.getTerritories()) {
			int advisors = view.map.getAdvisorUnitsInTerritory(terr.name, beneIndex);
			if (advisors <= 0) continue;

			bool doFlip = false;
			if (view.interactiveMode) {
				if (ctx.logger) {
					ctx.logger->logDebug("[Bene Gesserit] Declare battle from advisors in " + terr.name +
						" and flip to fighters? (y/n): ");
				}
				while (true) {
					std::string input;
					std::getline(std::cin >> std::ws, input);
					if (input == "y" || input == "Y" || input == "yes" || input == "Yes") {
						doFlip = true;
						break;
					}
					if (input == "n" || input == "N" || input == "no" || input == "No") {
						break;
					}
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid input. Enter y or n: ");
					}
				}
			} else {
				doFlip = (view.map.countCombatFactionsInTerritory(terr.name) > 0);
			}

			if (doFlip) {
				beneAbility->flipAdvisorsToFighters(ctx, terr.name);
			}
		}
	}

	// Atreides prescience is a base faction ability.
	if (ctx.logger) {
		for (size_t i = 0; i < ctx.players.size(); ++i) {
			FactionAbility* ability = ctx.getAbility(static_cast<int>(i));
			if (!ability || ability->getFactionName() != "Atreides") continue;

			const int blowCount = ctx.spiceDeck.isUsingExtendedSpiceBlow() ? 2 : 1;
			auto upcoming = ctx.spiceDeck.peekNextCards(blowCount);
			for (size_t cardIdx = 0; cardIdx < upcoming.size(); ++cardIdx) {
				const spiceCard& c = upcoming[cardIdx];
				if (c.type == spiceCardType::WORM) {
					ctx.logger->logDebug("[Atreides Prescience] Next Spice Blow card " + std::to_string(cardIdx + 1) + ": WORM");
				} else {
					ctx.logger->logDebug("[Atreides Prescience] Next Spice Blow card " + std::to_string(cardIdx + 1) + ": " +
						c.territoryName + " (" + std::to_string(c.spiceAmount) + " spice)");
				}
			}
			break;
		}
	}

	auto executeShipmentForPlayer = [&](int playerIndex) {
		Player* player = view.players[playerIndex];
		if (ctx.logger) {
			ctx.logger->logDebug("--- " + player->getFactionName() + " Shipment ---");
		}
		bool didShipment = executePlayerShipment(ctx, player);
		didAny[playerIndex] = didAny[playerIndex] || didShipment;
	};

	auto executeMovementForPlayer = [&](int playerIndex) {
		Player* player = view.players[playerIndex];
		if (ctx.logger) {
			ctx.logger->logDebug("--- " + player->getFactionName() + " Movement ---");
		}
		bool didMovement = executePlayerMovement(ctx, player);
		didAny[playerIndex] = didAny[playerIndex] || didMovement;
	};

	auto runShipmentStep = [&](const std::vector<int>& order) {
		for (int playerIndex : order) {
			executeShipmentForPlayer(playerIndex);
		}
	};

	auto runMovementStep = [&](const std::vector<int>& order) {
		for (int playerIndex : order) {
			executeMovementForPlayer(playerIndex);
		}
	};

	auto logPasses = [&]() {
		for (int playerIndex : view.turnOrder) {
			if (!didAny[playerIndex] && ctx.logger) {
				ctx.logger->logDebug(view.players[playerIndex]->getFactionName() + " passes (no actions taken)");
			}
		}
	};

	int guildIndex = -1;
	if (ctx.featureSettings.advancedFactionAbilities) {
		for (int playerIndex : view.turnOrder) {
			FactionAbility* ability = ctx.getAbility(playerIndex);
			if (ability && ability->canMoveOutOfTurnOrder()) {
				guildIndex = playerIndex;
				break;
			}
		}
	}

	if (guildIndex == -1) {
		// Standard flow: each faction resolves shipment, then movement.
		for (int playerIndex : view.turnOrder) {
			executeShipmentForPlayer(playerIndex);
			executeMovementForPlayer(playerIndex);
		}
		logPasses();
		return;
	}

	std::vector<int> nonGuildOrder = view.turnOrder;
	nonGuildOrder.erase(std::remove(nonGuildOrder.begin(), nonGuildOrder.end(), guildIndex), nonGuildOrder.end());

	if (!view.interactiveMode) {
		runShipmentStep(nonGuildOrder);
		executeShipmentForPlayer(guildIndex);
		executeMovementForPlayer(guildIndex);
		runMovementStep(nonGuildOrder);
		logPasses();
		return;
	}

	bool guildShipmentTaken = false;
	bool guildMovementTaken = false;

	auto askYesNo = [&](const std::string& prompt) {
		std::string answer;
		std::cout << prompt;
		std::cin >> answer;
		return (answer == "y" || answer == "Y" || answer == "yes" || answer == "Yes");
	};

	auto offerGuildInterrupt = [&](const std::string& whenLabel) {
		if (!guildShipmentTaken) {
			if (askYesNo("  [Guild] Do you want to ship now (" + whenLabel + ")? (y/n): ")) {
				executeShipmentForPlayer(guildIndex);
				guildShipmentTaken = true;
				if (!guildMovementTaken &&
					askYesNo("  [Guild] Do you also want to move now? (y/n): ")) {
					executeMovementForPlayer(guildIndex);
					guildMovementTaken = true;
				}
			}
		} else if (!guildMovementTaken) {
			if (askYesNo("  [Guild] Do you want to move now (" + whenLabel + ")? (y/n): ")) {
				executeMovementForPlayer(guildIndex);
				guildMovementTaken = true;
			}
		}
	};

	// Offer before first non-Guild shipment.
	offerGuildInterrupt("before first shipment");

	for (int playerIndex : nonGuildOrder) {
		Player* player = view.players[playerIndex];
		executeShipmentForPlayer(playerIndex);

		offerGuildInterrupt("after " + player->getFactionName() + " shipment");

		executeMovementForPlayer(playerIndex);

		offerGuildInterrupt("after " + player->getFactionName() + " movement");
	}

	// Final optional chance if Guild still has pending action.
	if (!guildShipmentTaken || !guildMovementTaken) {
		offerGuildInterrupt("end of phase");
	}
	logPasses();
}

// ============================================================================
// SHIPMENT (DEPLOYMENT)
// ============================================================================

bool ShipAndMovePhase::executePlayerShipment(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();
	FactionAbility* ability = player->getFactionAbility();
	bool isGuild = ability && ability->canCrossShip() && ability->canShipToReserves();

	if (isGuild && view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug("Shipment type:");
			ctx.logger->logDebug("  0. Skip");
			ctx.logger->logDebug("  1. Normal shipment (reserve -> Dune)");
			ctx.logger->logDebug("  2. Guild cross-shipment (territory -> territory)");
			ctx.logger->logDebug("  3. Guild return shipment (territory -> reserves)");
			ctx.logger->logDebug("Choose shipment type (0-3): ");
		}

		int shipmentType = -1;
		readIntInRange(ctx, 0, 3, shipmentType);

		if (shipmentType == 0) {
			if (ctx.logger) ctx.logger->logDebug("Shipment: Skipped");
			return false;
		}
		if (shipmentType == 2) {
			return executeGuildCrossShipment(ctx, player);
		}
		if (shipmentType == 3) {
			return executeGuildReturnToReserveShipment(ctx, player);
		}
	}

	if (player->getUnitsReserve() <= 0) {
		if (ctx.logger) ctx.logger->logDebug("Shipment: No units in reserve");
		return false;
	}
	
	std::vector<std::string> validTargets = getValidDeploymentTargets(ctx, player->getFactionIndex());
	
	DeploymentDecision decision;
	
	if (view.interactiveMode) {
		auto choice = InteractiveInput::getDeploymentDecision(ctx, player, validTargets);
		decision.territoryName = choice.territoryName;
		decision.normalUnits   = choice.normalUnits;
		decision.eliteUnits    = choice.eliteUnits;
		decision.shouldDeploy  = choice.shouldDeploy;
		decision.sector        = choice.sector;  // Use player's sector choice
		decision.spiceCost     = 0;
	} else {
		decision = aiDecideDeployment(ctx, player);
	}
	
	if (!decision.shouldDeploy) {
		if (ctx.logger) ctx.logger->logDebug("Shipment: Skipped");
		return false;
	}

	// Resolve sector if not set (AI mode or invalid choice)
	if (decision.sector == -1) {
		const territory* dest = view.map.getTerritory(decision.territoryName);
		decision.sector = GameMap::firstSafeSector(dest, ctx.stormSector);
	}
	
	return deployUnits(ctx, player, decision.territoryName,
	                   decision.normalUnits, decision.eliteUnits, decision.sector);
}

bool ShipAndMovePhase::executeGuildCrossShipment(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();
	int factionIndex = player->getFactionIndex();

	GuildSourceSelection source;
	if (!selectGuildSource(ctx, factionIndex, "Guild cross-shipment", source)) {
		return false;
	}

	int normalUnits = 0;
	int eliteUnits = 0;
	if (!selectUnitSplitFromSource(ctx, source.normalAvailable, source.eliteAvailable,
		"How many units to ship?", normalUnits, eliteUnits)) {
		return false;
	}
	int totalUnits = normalUnits + eliteUnits;

	std::vector<std::string> destinationTerritories;
	for (const auto& terr : view.map.getTerritories()) {
		if (terr.name == source.territory) continue;
		if (!GameMap::canEnterTerritory(&terr, ctx.stormSector)) continue;
		if (!view.map.canAddFactionToTerritory(terr.name, factionIndex)) continue;
		destinationTerritories.push_back(terr.name);
	}
	if (destinationTerritories.empty()) {
		if (ctx.logger) ctx.logger->logDebug("Guild cross-shipment FAILED: no valid destination territory");
		return false;
	}

	int destIdx = promptMenuChoice(ctx, "Choose destination territory (0 to cancel):", destinationTerritories);
	if (destIdx < 0) return false;
	std::string toTerritory = destinationTerritories[destIdx];

	const territory* dest = view.map.getTerritory(toTerritory);
	if (!dest) return false;

	std::vector<int> safeDestSectors;
	for (int s : dest->sectors) {
		if (GameMap::canLeaveSector(s, ctx.stormSector)) {
			safeDestSectors.push_back(s);
		}
	}
	if (safeDestSectors.empty()) {
		if (ctx.logger) ctx.logger->logDebug("Guild cross-shipment FAILED: destination fully in storm");
		return false;
	}

	int toSector = safeDestSectors[0];
	if (safeDestSectors.size() > 1) {
		std::vector<std::string> sectorOptions;
		for (int s : safeDestSectors) {
			sectorOptions.push_back("Sector " + std::to_string(s));
		}
		int sectorIdx = promptMenuChoice(ctx, "Choose destination sector (0 to cancel):", sectorOptions);
		if (sectorIdx < 0) return false;
		toSector = safeDestSectors[sectorIdx];
	}

	view.map.removeUnitsFromTerritorySector(source.territory, factionIndex, normalUnits, eliteUnits, source.sector);
	view.map.addUnitsToTerritory(toTerritory, factionIndex, normalUnits, eliteUnits, toSector);

	if (ctx.logger) {
		ctx.logger->logDebug("Guild cross-shipment: " + std::to_string(totalUnits) + " units " +
			source.territory + "(s" + std::to_string(source.sector) + ") -> " +
			toTerritory + "(s" + std::to_string(toSector) + ")");
	}

	return true;
}

bool ShipAndMovePhase::executeGuildReturnToReserveShipment(PhaseContext& ctx, Player* player) {
	int factionIndex = player->getFactionIndex();

	GuildSourceSelection source;
	if (!selectGuildSource(ctx, factionIndex, "Guild return shipment", source)) {
		return false;
	}

	int normalUnits = 0;
	int eliteUnits = 0;
	if (!selectUnitSplitFromSource(ctx, source.normalAvailable, source.eliteAvailable,
		"How many units to ship to reserves?", normalUnits, eliteUnits)) {
		return false;
	}
	int totalUnits = normalUnits + eliteUnits;

	auto view = ctx.getShipAndMoveView();
	view.map.removeUnitsFromTerritorySector(source.territory, factionIndex, normalUnits, eliteUnits, source.sector);
	player->recallUnits(normalUnits);
	player->recallEliteUnits(eliteUnits);

	if (ctx.logger) {
		ctx.logger->logDebug("Guild return shipment: " + std::to_string(totalUnits) + " units from " +
			source.territory + "(s" + std::to_string(source.sector) + ") -> reserves");
	}

	return true;
}

int ShipAndMovePhase::calculateDeploymentCost(const territory* terr, int unitCount, Player* player) const {
	if (terr == nullptr || unitCount <= 0 || player == nullptr) return 0;
	
	FactionAbility* ability = player->getFactionAbility();
	if (ability) return ability->getShipmentCost(terr, unitCount);
	
	if (terr->terrain == terrainType::city) return unitCount;
	return unitCount * 2;
}

std::vector<std::string> ShipAndMovePhase::getValidDeploymentTargets(
	PhaseContext& ctx, int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();
	std::vector<std::string> validTargets;
	bool beneAdvanced = false;
	if (ctx.featureSettings.advancedFactionAbilities) {
		beneAdvanced = (dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(factionIndex)) != nullptr);
	}
	
	FactionAbility* ability = ctx.getAbility(factionIndex);
	std::vector<std::string> restricted;
	if (ability) restricted = ability->getValidDeploymentTerritories(ctx);
	
	auto isValid = [&](const territory& terr) -> bool {
		// Cannot ship into a sector that is fully in storm.
		if (!GameMap::canEnterTerritory(&terr, ctx.stormSector)) return false;
		if (beneAdvanced) return true;
		return view.map.canAddFactionToTerritory(terr.name, factionIndex);
	};

	if (!restricted.empty()) {
		for (const auto& name : restricted) {
			const territory* terr = view.map.getTerritory(name);
			if (terr && isValid(*terr)) validTargets.push_back(name);
		}
	} else {
		for (const auto& terr : view.map.getTerritories()) {
			if (isValid(terr)) validTargets.push_back(terr.name);
		}
	}
	
	return validTargets;
}

bool ShipAndMovePhase::deployUnits(PhaseContext& ctx, Player* player,
                                    const std::string& territoryName,
                                    int normalUnits, int eliteUnits, int sector) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	bool asAdvisor = false;
	if (ctx.featureSettings.advancedFactionAbilities) {
		auto* bg = dynamic_cast<BeneGesseritAbility*>(player->getFactionAbility());
		if (bg) {
			bg->beginTurn(ctx.turnNumber);
			const bool unoccupiedByCombatForces = (view.map.countCombatFactionsInTerritory(territoryName) == 0);
			asAdvisor = !unoccupiedByCombatForces;
		}
	}

	if (asAdvisor) {
		territory* checkTerr = view.map.getTerritory(territoryName);
		if (totalUnits <= 0 || checkTerr == nullptr || !GameMap::canEnterTerritory(checkTerr, ctx.stormSector) ||
			!view.map.canAddAdvisorToTerritory(territoryName, player->getFactionIndex())) {
			if (ctx.logger) ctx.logger->logDebug("Deployment FAILED: Invalid advisor deployment");
			return false;
		}
	} else if (!isValidDeployment(ctx, player->getFactionIndex(), territoryName, totalUnits)) {
		if (ctx.logger) ctx.logger->logDebug("Deployment FAILED: Invalid deployment");
		return false;
	}
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) return false;
	
	// Resolve sector if not set.
	int effectiveSector = sector;
	if (effectiveSector == -1) {
		effectiveSector = GameMap::firstSafeSector(terr, ctx.stormSector);
	}
	if (effectiveSector == -1) {
		if (ctx.logger) ctx.logger->logDebug("Deployment FAILED: No safe sector available");
		return false;
	}

	int cost = calculateDeploymentCost(terr, totalUnits, player);
	if (player->getSpice() < cost) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Need " + std::to_string(cost) +
				" spice, have " + std::to_string(player->getSpice()));
		}
		return false;
	}
	
	if (player->getUnitsReserve() < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Deployment FAILED: Need " + std::to_string(totalUnits) +
				" units in reserve, have " + std::to_string(player->getUnitsReserve()));
		}
		return false;
	}

	player->removeSpice(cost);
	player->deployUnits(totalUnits);
	view.map.addUnitsToTerritory(territoryName, player->getFactionIndex(),
	                              normalUnits, eliteUnits, effectiveSector, asAdvisor);

	// Notify factions that a shipment payment was made (Guild hook + Bene advisors).
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		FactionAbility* ability = ctx.getAbility(static_cast<int>(i));
		if (ability) {
			ability->onOtherFactionShipped(ctx, player->getFactionIndex(), cost, territoryName, true);
		}
	}

	if (ctx.featureSettings.advancedFactionAbilities) {
		for (size_t i = 0; i < ctx.players.size(); ++i) {
			auto* bg = dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(static_cast<int>(i)));
			if (!bg) continue;
			if (static_cast<int>(i) == player->getFactionIndex()) continue;
			if (view.map.getCombatUnitsInTerritory(territoryName, static_cast<int>(i)) <= 0) continue;
			if (ctx.logger) {
				ctx.logger->logDebug("[Bene Gesserit] Intrusion in " + territoryName +
					": flip fighters to advisors? (y/n): ");
			}
			bool flip = !view.interactiveMode;
			if (view.interactiveMode) {
				while (true) {
					std::string input;
					std::getline(std::cin >> std::ws, input);
					if (input == "y" || input == "Y" || input == "yes" || input == "Yes") {
						flip = true;
						break;
					}
					if (input == "n" || input == "N" || input == "no" || input == "No") {
						flip = false;
						break;
					}
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid input. Enter y or n: ");
					}
				}
			}
			if (flip) {
				bg->flipFightersToAdvisors(ctx, territoryName);
			}
		}
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Shipment: " + std::to_string(totalUnits) +
			" units → " + territoryName +
			" (sector " + std::to_string(effectiveSector) + ")" +
			(std::string(" [") + (asAdvisor ? "advisor" : "fighter") + "]") +
			" for " + std::to_string(cost) + " spice");

		Event e(EventType::UNITS_SHIPPED,
			"Deployed " + std::to_string(totalUnits) + " units to " + territoryName,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = player->getFactionName();
		e.territory     = territoryName;
		e.unitCount     = totalUnits;
		e.spiceValue    = cost;
		ctx.logger->logEvent(e);
	}
	
	return true;
}

bool ShipAndMovePhase::isValidDeployment(PhaseContext& ctx, int factionIndex,
                                          const std::string& territoryName,
                                          int unitCount) const {
	auto view = ctx.getShipAndMoveView();

	if (unitCount <= 0) return false;
	
	territory* terr = view.map.getTerritory(territoryName);
	if (terr == nullptr) return false;
	if (!GameMap::canEnterTerritory(terr, ctx.stormSector)) return false;
	
	return view.map.canAddFactionToTerritory(territoryName, factionIndex);
}

// ============================================================================
// MOVEMENT
// ============================================================================

bool ShipAndMovePhase::executePlayerMovement(PhaseContext& ctx, Player* player) {
	auto view = ctx.getShipAndMoveView();

	int movementRange = calculateMovementRange(ctx, player->getFactionIndex());
	
	std::vector<std::string> territoriesWithUnits =
		view.map.getTerritoriesWithUnits(player->getFactionIndex());
	
	if (territoriesWithUnits.empty()) {
		if (ctx.logger) ctx.logger->logDebug("Movement: No units on map");
		return false;
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Movement range: " + std::to_string(movementRange) +
			(movementRange == 3 ? " (ornithopter)" : ""));
	}
	
	MovementDecision decision;
	
	if (view.interactiveMode) {
		auto choice = InteractiveInput::getMovementDecision(ctx, player, territoriesWithUnits, movementRange);
		decision.fromTerritory = choice.fromTerritory;
		decision.toTerritory   = choice.toTerritory;
		decision.normalUnits   = choice.normalUnits;
		decision.eliteUnits    = choice.eliteUnits;
		decision.shouldMove    = choice.shouldMove;
		decision.fromSector    = choice.fromSector;  // Use player's sector choice
		decision.toSector      = choice.toSector;    // Use player's sector choice
	} else {
		decision = aiDecideMovement(ctx, player, movementRange);
	}
	
	if (!decision.shouldMove) {
		if (ctx.logger) ctx.logger->logDebug("Movement: Skipped");
		return false;
	}

	// Resolve sectors if not set by AI/interactive.
	if (decision.fromSector == -1) {
		// Pick any sector of the source that is not in storm.
		const territory* src = view.map.getTerritory(decision.fromTerritory);
		if (src) {
			for (int s : src->sectors) {
				if (GameMap::canLeaveSector(s, ctx.stormSector) &&
				    view.map.getUnitsInTerritorySector(decision.fromTerritory,
				                                        player->getFactionIndex(), s) > 0) {
					decision.fromSector = s;
					break;
				}
			}
		}
	}
	if (decision.toSector == -1) {
		const territory* dest = view.map.getTerritory(decision.toTerritory);
		decision.toSector = GameMap::firstSafeSector(dest, ctx.stormSector);
	}

	if (decision.fromSector == -1 || decision.toSector == -1) {
		if (ctx.logger) ctx.logger->logDebug("Movement FAILED: Could not resolve sectors");
		return false;
	}
	
	return moveUnits(ctx, player->getFactionIndex(),
	                 decision.fromTerritory, decision.fromSector,
	                 decision.toTerritory,   decision.toSector,
	                 decision.normalUnits,   decision.eliteUnits);
}

int ShipAndMovePhase::calculateMovementRange(PhaseContext& ctx, int factionIndex) const {
	auto view = ctx.getShipAndMoveView();

	FactionAbility* ability = ctx.getAbility(factionIndex);
	int baseRange = ability ? ability->getBaseMovementRange() : 1;

	bool controlsArrakeen = view.map.isControlled("Arrakeen", factionIndex);
	bool controlsCarthag  = view.map.isControlled("Carthag",  factionIndex);
	
	return (controlsArrakeen || controlsCarthag) ? 3 : baseRange;
}

std::vector<std::string> ShipAndMovePhase::getReachableTerritories(
	PhaseContext& ctx,
	const std::string& sourceTerritoryName,
	int sourceUnitSector,
	int movementRange,
	int factionIndex) const {
	
	auto view = ctx.getShipAndMoveView();
	std::vector<std::string> reachable;
	
	// Units cannot leave if their sector is in storm.
	if (!GameMap::canLeaveSector(sourceUnitSector, ctx.stormSector)) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement blocked: units in sector " +
				std::to_string(sourceUnitSector) + " cannot leave (storm)");
		}
		return reachable;
	}

	const territory* source = view.map.getTerritory(sourceTerritoryName);
	if (source == nullptr) return reachable;
	
	// BFS: visit territories within movementRange steps.
	// An adjacent territory is passable only if canEnterTerritory() is true.
	std::queue<std::pair<const territory*, int>> queue;
	std::set<std::string> visited;
	
	queue.push({source, 0});
	visited.insert(source->name);
	
	while (!queue.empty()) {
		auto [currentTerr, distance] = queue.front();
		queue.pop();
		
		if (distance > 0 &&
		    GameMap::canEnterTerritory(currentTerr, ctx.stormSector) &&
		    view.map.canAddFactionToTerritory(currentTerr->name, factionIndex)) {
			reachable.push_back(currentTerr->name);
		}
		
		if (distance >= movementRange) continue;
		
		for (const territory* neighbor : currentTerr->neighbourPtrs) {
			if (visited.count(neighbor->name)) continue;
			// Cannot pass through a territory that is entirely in storm.
			// (Units can move through it only if it has a safe sector to traverse.)
			if (!GameMap::canEnterTerritory(neighbor, ctx.stormSector)) {
				visited.insert(neighbor->name);
				continue;  // Blocked — do not explore through this territory.
			}
			visited.insert(neighbor->name);
			queue.push({neighbor, distance + 1});
		}
	}
	
	return reachable;
}

bool ShipAndMovePhase::moveUnits(PhaseContext& ctx, int factionIndex,
                                  const std::string& fromTerritory, int fromSector,
                                  const std::string& toTerritory,   int toSector,
                                  int normalUnits, int eliteUnits) {
	auto view = ctx.getShipAndMoveView();

	int totalUnits = normalUnits + eliteUnits;
	int unitsInSector = view.map.getUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	int combatUnitsInSector = view.map.getCombatUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	int advisorUnitsInSector = view.map.getAdvisorUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	bool beneAdvanced = ctx.featureSettings.advancedFactionAbilities &&
		(dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(factionIndex)) != nullptr);

	if (unitsInSector < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Need " + std::to_string(totalUnits) +
				" in sector " + std::to_string(fromSector) + " of " + fromTerritory +
				", have " + std::to_string(unitsInSector));
		}
		return false;
	}
	if (!beneAdvanced && combatUnitsInSector < totalUnits) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Only fighters may move for this faction in sector " +
				std::to_string(fromSector) + " of " + fromTerritory);
		}
		return false;
	}

	int eliteInSector = view.map.getEliteUnitsInTerritorySector(fromTerritory, factionIndex, fromSector);
	int normalInSector = combatUnitsInSector - eliteInSector;
	if (normalUnits < 0 || eliteUnits < 0 || normalUnits > normalInSector || eliteUnits > eliteInSector) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: Invalid unit split from sector " +
				std::to_string(fromSector) + " of " + fromTerritory + " (requested " +
				std::to_string(normalUnits) + " normal, " + std::to_string(eliteUnits) +
				" elite; available " + std::to_string(normalInSector) + " normal, " +
				std::to_string(eliteInSector) + " elite)");
		}
		if (!(beneAdvanced && eliteUnits == 0 && advisorUnitsInSector >= totalUnits)) {
			return false;
		}
	}

	// Validate reachability using the source sector.
	int movementRange = calculateMovementRange(ctx, factionIndex);
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, fromTerritory, fromSector, movementRange, factionIndex);
	
	if (std::find(reachable.begin(), reachable.end(), toTerritory) == reachable.end()) {
		if (ctx.logger) {
			ctx.logger->logDebug("Movement FAILED: " + toTerritory +
				" not reachable from sector " + std::to_string(fromSector) +
				" of " + fromTerritory);
		}
		return false;
	}
	
	bool movingAsAdvisor = false;
	if (ctx.featureSettings.advancedFactionAbilities) {
		auto* bene = dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(factionIndex));
		if (bene) {
			bene->beginTurn(ctx.turnNumber);
			if (eliteUnits == 0 && advisorUnitsInSector >= totalUnits && combatUnitsInSector < totalUnits) {
				movingAsAdvisor = true;
			}
		}
	}
	if (movingAsAdvisor) {
		eliteUnits = 0;
		normalUnits = totalUnits;
	}

	// Remove from source sector, add to destination sector.
	if (movingAsAdvisor) {
		view.map.removeUnitsFromTerritorySector(fromTerritory, factionIndex,
		                                         normalUnits, eliteUnits, fromSector, true);
	} else {
		view.map.removeUnitsFromTerritorySector(fromTerritory, factionIndex,
		                                         normalUnits, eliteUnits, fromSector, false);
	}

	if (movingAsAdvisor) {
		bool destinationHasCombat = (view.map.countCombatFactionsInTerritory(toTerritory) > 0);
		bool keepAdvisor = destinationHasCombat;
		if (destinationHasCombat && view.interactiveMode) {
			if (ctx.logger) {
				ctx.logger->logDebug("[Bene Gesserit] Keep moved units as advisors in " + toTerritory + "? (y/n): ");
			}
			while (true) {
				std::string input;
				std::getline(std::cin >> std::ws, input);
				if (input == "y" || input == "Y" || input == "yes" || input == "Yes") {
					keepAdvisor = true;
					break;
				}
				if (input == "n" || input == "N" || input == "no" || input == "No") {
					keepAdvisor = false;
					break;
				}
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Enter y or n: ");
				}
			}
		}
		view.map.addUnitsToTerritory(toTerritory, factionIndex,
		                              normalUnits, eliteUnits, toSector, keepAdvisor);
	} else {
		view.map.addUnitsToTerritory(toTerritory, factionIndex,
		                              normalUnits, eliteUnits, toSector, false);
	}

	if (ctx.featureSettings.advancedFactionAbilities) {
		for (size_t i = 0; i < ctx.players.size(); ++i) {
			auto* bg = dynamic_cast<BeneGesseritAbility*>(ctx.getAbility(static_cast<int>(i)));
			if (!bg) continue;
			if (static_cast<int>(i) == factionIndex) continue;
			if (view.map.getCombatUnitsInTerritory(toTerritory, static_cast<int>(i)) <= 0) continue;
			if (ctx.logger) {
				ctx.logger->logDebug("[Bene Gesserit] Intrusion in " + toTerritory +
					": flip fighters to advisors? (y/n): ");
			}
			bool flip = !view.interactiveMode;
			if (view.interactiveMode) {
				while (true) {
					std::string input;
					std::getline(std::cin >> std::ws, input);
					if (input == "y" || input == "Y" || input == "yes" || input == "Yes") {
						flip = true;
						break;
					}
					if (input == "n" || input == "N" || input == "no" || input == "No") {
						flip = false;
						break;
					}
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid input. Enter y or n: ");
					}
				}
			}
			if (flip) {
				bg->flipFightersToAdvisors(ctx, toTerritory);
			}
		}
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug("Moved " + std::to_string(totalUnits) +
			" units: " + fromTerritory + "(s" + std::to_string(fromSector) + ")" +
			" → " + toTerritory + "(s" + std::to_string(toSector) + ")");
		
		Event e(EventType::UNITS_MOVED,
			"Moved " + std::to_string(totalUnits) +
			" units from " + fromTerritory + " to " + toTerritory,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = view.players[factionIndex]->getFactionName();
		e.territory     = toTerritory;
		e.unitCount     = totalUnits;
		ctx.logger->logEvent(e);
	}
	
	return true;
}

bool ShipAndMovePhase::isValidMovement(PhaseContext& ctx, int factionIndex,
                                        const std::string& fromTerritory,
                                        int fromSector,
                                        const std::string& toTerritory,
                                        int movementRange) const {
	std::vector<std::string> reachable = getReachableTerritories(
		ctx, fromTerritory, fromSector, movementRange, factionIndex);
	return std::find(reachable.begin(), reachable.end(), toTerritory) != reachable.end();
}

// ============================================================================
// AI DECISIONS
// ============================================================================

ShipAndMovePhase::DeploymentDecision ShipAndMovePhase::aiDecideDeployment(
	PhaseContext& ctx, Player* player) const {
	
	auto view = ctx.getShipAndMoveView();

	DeploymentDecision decision;
	decision.shouldDeploy = false;
	decision.normalUnits  = 0;
	decision.eliteUnits   = 0;
	decision.sector       = -1;
	decision.spiceCost    = 0;
	
	if (player->getSpice() < 4 || player->getUnitsReserve() <= 0) return decision;
	
	std::vector<std::string> validTargets =
		getValidDeploymentTargets(ctx, player->getFactionIndex());
	if (validTargets.empty()) return decision;
	
	// Prefer cities (cheapest), otherwise first valid territory.
	std::string targetTerritory;
	for (const auto& name : validTargets) {
		const territory* terr = view.map.getTerritory(name);
		if (terr && terr->terrain == terrainType::city) {
			targetTerritory = name;
			break;
		}
	}
	if (targetTerritory.empty()) targetTerritory = validTargets[0];
	
	const territory* terr = view.map.getTerritory(targetTerritory);
	if (terr == nullptr) return decision;

	int safeSector = GameMap::firstSafeSector(terr, ctx.stormSector);
	if (safeSector == -1) return decision;  // territory fully in storm
	
	int cost = calculateDeploymentCost(terr, 1, player);
	int maxAffordable = (cost == 0) ? player->getUnitsReserve()
	                                 : player->getSpice() / cost;
	int unitsToDeploy = std::min({3, maxAffordable, player->getUnitsReserve()});
	if (unitsToDeploy <= 0) return decision;
	
	decision.shouldDeploy   = true;
	decision.territoryName  = targetTerritory;
	decision.normalUnits    = unitsToDeploy;
	decision.eliteUnits     = 0;
	decision.sector         = safeSector;
	decision.spiceCost      = calculateDeploymentCost(terr, unitsToDeploy, player);
	
	return decision;
}

ShipAndMovePhase::MovementDecision ShipAndMovePhase::aiDecideMovement(
	PhaseContext& ctx, Player* player, int movementRange) const {
	
	auto view = ctx.getShipAndMoveView();

	MovementDecision decision;
	decision.shouldMove  = false;
	decision.normalUnits = 0;
	decision.eliteUnits  = 0;
	decision.fromSector  = -1;
	decision.toSector    = -1;
	
	std::vector<std::string> territoriesWithUnits =
		view.map.getTerritoriesWithUnits(player->getFactionIndex());
	if (territoriesWithUnits.empty()) return decision;
	
	// Try each source territory until we find one with a movable stack.
	for (const auto& sourceName : territoriesWithUnits) {
		const territory* src = view.map.getTerritory(sourceName);
		if (!src) continue;

		// Find a sector in this territory that has units and can leave.
		int movableSector = -1;
		int unitsToMove   = 0;
		for (const auto& stack : src->unitsPresent) {
			if (stack.factionOwner != player->getFactionIndex()) continue;
			if (!GameMap::canLeaveSector(stack.sector, ctx.stormSector)) continue;
			int count = stack.normal_units + stack.elite_units;
			if (count > 0) {
				movableSector = stack.sector;
				unitsToMove   = std::max(1, count / 2);
				break;
			}
		}
		if (movableSector == -1) continue;

		std::vector<std::string> reachable =
			getReachableTerritories(ctx, sourceName, movableSector, movementRange,
			                        player->getFactionIndex());
		if (reachable.empty()) continue;

		// Prefer spice-rich territory, then city, then first reachable.
		std::string best;
		int bestSpice = -1;
		for (const auto& name : reachable) {
			const territory* t = view.map.getTerritory(name);
			if (t) {
				int spiceHere = view.map.getSpiceInTerritory(name);
				if (spiceHere > bestSpice) {
					bestSpice = spiceHere;
					best = name;
				}
			}
		}
		if (best.empty()) {
			for (const auto& name : reachable) {
				const territory* t = view.map.getTerritory(name);
				if (t && t->terrain == terrainType::city) { best = name; break; }
			}
		}
		if (best.empty()) best = reachable[0];

		const territory* dest = view.map.getTerritory(best);
		int toSector = GameMap::firstSafeSector(dest, ctx.stormSector);
		if (toSector == -1) continue;

		decision.shouldMove    = true;
		decision.fromTerritory = sourceName;
		decision.fromSector    = movableSector;
		decision.toTerritory   = best;
		decision.toSector      = toSector;
		decision.normalUnits   = unitsToMove;
		decision.eliteUnits    = 0;
		return decision;
	}
	
	return decision;
}