#include "factions/bene_gesserit_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "logger/event_logger.hpp"
#include "events/event.hpp"
#include "settings.hpp"
#include "map.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>

std::string BeneGesseritAbility::getFactionName() const { return "Bene Gesserit"; }

void BeneGesseritAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(5);
	player->setUnitsReserve(19);
	player->setEliteUnitsReserve(0);
}

void BeneGesseritAbility::placeStartingForces(PhaseContext& ctx) {
	int beneIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		FactionAbility* ability = ctx.players[i]->getFactionAbility();
		if (ability && ability->getFactionName() == "Bene Gesserit") {
			beneIndex = static_cast<int>(i);
			break;
		}
	}

	if (beneIndex < 0) return;

	Player* bene = ctx.players[beneIndex];
	if (bene->getUnitsReserve() <= 0) {
		choosePrediction(ctx, beneIndex);
		return;
	}

	if (ctx.featureSettings.advancedFactionAbilities) {
		std::string chosenTerritory;
		const auto& territories = ctx.map.getTerritories();

		if (ctx.interactiveMode) {
			if (ctx.logger) {
				ctx.logger->logDebug("=== BENE GESSERIT ADVANCED START PLACEMENT ===");
				ctx.logger->logDebug("After Fremen placement, choose one territory for your starting peaceful advisor:");
				for (size_t i = 0; i < territories.size(); ++i) {
					ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + territories[i].name);
				}
				ctx.logger->logDebug("Choose territory by number or exact name: ");
			}

			while (true) {
				std::string input;
				std::getline(std::cin >> std::ws, input);

				if (input.empty()) {
					if (ctx.logger) ctx.logger->logDebug("Invalid choice. Try again: ");
					continue;
				}

				bool matched = false;
				try {
					int idx = std::stoi(input);
					if (idx >= 1 && idx <= static_cast<int>(territories.size())) {
						chosenTerritory = territories[static_cast<size_t>(idx - 1)].name;
						matched = true;
					}
				} catch (...) {
				}

				if (!matched) {
					for (const auto& terr : territories) {
						if (terr.name == input) {
							chosenTerritory = terr.name;
							matched = true;
							break;
						}
					}
				}

				if (matched && ctx.map.canAddFactionToTerritory(chosenTerritory, beneIndex)) {
					break;
				}

				if (ctx.logger) {
					ctx.logger->logDebug("Invalid territory (or occupied by two other factions). Try again: ");
				}
			}
		} else {
			for (const auto& terr : territories) {
				if (ctx.map.canAddFactionToTerritory(terr.name, beneIndex) &&
					ctx.map.countFactionsInTerritory(terr.name) == 0) {
					chosenTerritory = terr.name;
					break;
				}
			}

			if (chosenTerritory.empty()) {
				for (const auto& terr : territories) {
					if (ctx.map.canAddFactionToTerritory(terr.name, beneIndex)) {
						chosenTerritory = terr.name;
						break;
					}
				}
			}
		}

		if (!chosenTerritory.empty()) {
			bool aloneBeforePlacement = (ctx.map.countFactionsInTerritory(chosenTerritory) == 0);
			ctx.map.addUnitsToTerritory(chosenTerritory, beneIndex, 1, 0, -1, !aloneBeforePlacement);
			bene->deployUnits(1);

			if (ctx.logger) {
				if (aloneBeforePlacement) {
					ctx.logger->logDebug("[Bene Gesserit][Advanced] Starting advisor placed in " + chosenTerritory +
						" and flips to fighter (territory was empty)");
				} else {
					ctx.logger->logDebug("[Bene Gesserit][Advanced] Starting peaceful advisor placed in " + chosenTerritory);
				}
			}
		}
	} else {
		ctx.map.addUnitsToTerritory("Polar Sink", beneIndex, 1, 0, -1);
		bene->deployUnits(1);

		if (ctx.logger) {
			ctx.logger->logDebug("[Bene Gesserit] Starts with 1 force in Polar Sink, 19 in reserves, and 5 spice");
		}
	}

	choosePrediction(ctx, beneIndex);
}

int BeneGesseritAbility::getFreeRevivalsPerTurn() const {
	return 1;
}

bool BeneGesseritAbility::alwaysReceivesCharity() const {
	return true;
}

bool BeneGesseritAbility::canUseWorthlessAsKarama() const {
	return true;
}

void BeneGesseritAbility::beginTurn(int turnNumber) {
	if (lockedTurn != turnNumber) {
		lockedTurn = turnNumber;
		lockedAdvisorTerritories.clear();
	}
}

bool BeneGesseritAbility::canFlipAdvisorsToFightersThisTurn(PhaseContext& ctx, const std::string& territoryName) const {
	if (lockedTurn != ctx.turnNumber || lockedAdvisorTerritories.count(territoryName) == 0) {
		return true;
	}

	const territory* terr = ctx.map.getTerritory(territoryName);
	if (!terr) return false;

	for (const auto& stack : terr->unitsPresent) {
		if (!stack.advisor && (stack.normal_units > 0 || stack.elite_units > 0)) {
			return false;
		}
	}
	return true;
}

int BeneGesseritAbility::flipAdvisorsToFighters(PhaseContext& ctx, const std::string& territoryName) {
	if (!canFlipAdvisorsToFightersThisTurn(ctx, territoryName)) {
		if (ctx.logger) {
			ctx.logger->logDebug("[Bene Gesserit] Advisors in " + territoryName +
				" cannot flip this turn while other forces remain.");
		}
		return 0;
	}

	int beneIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		FactionAbility* ability = ctx.players[i]->getFactionAbility();
		if (ability && ability->getFactionName() == "Bene Gesserit") {
			beneIndex = static_cast<int>(i);
			break;
		}
	}
	if (beneIndex < 0) return 0;

	int flipped = ctx.map.flipAdvisorsToFighters(territoryName, beneIndex);
	if (flipped > 0 && ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] " + std::to_string(flipped) +
			" advisor(s) flip to fighters in " + territoryName);
	}
	return flipped;
}

int BeneGesseritAbility::flipFightersToAdvisors(PhaseContext& ctx, const std::string& territoryName) {
	int beneIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		FactionAbility* ability = ctx.players[i]->getFactionAbility();
		if (ability && ability->getFactionName() == "Bene Gesserit") {
			beneIndex = static_cast<int>(i);
			break;
		}
	}
	if (beneIndex < 0) return 0;

	int flipped = ctx.map.flipFightersToAdvisors(territoryName, beneIndex);
	if (flipped > 0 && ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] " + std::to_string(flipped) +
			" fighter(s) flip to advisors in " + territoryName);
	}
	return flipped;
}

void BeneGesseritAbility::onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount,
	const std::string& destinationTerritory, bool fromOffPlanet) {
	(void)amount;
	if (shippingFactionIndex < 0 || shippingFactionIndex >= static_cast<int>(ctx.players.size())) {
		return;
	}
	if (!ctx.featureSettings.advancedFactionAbilities || !fromOffPlanet) {
		return;
	}

	beginTurn(ctx.turnNumber);

	int beneIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		FactionAbility* ability = ctx.players[i]->getFactionAbility();
		if (ability && ability->getFactionName() == "Bene Gesserit") {
			beneIndex = static_cast<int>(i);
			break;
		}
	}

	if (beneIndex < 0 || shippingFactionIndex == beneIndex) {
		return;
	}

	Player* bene = ctx.players[beneIndex];
	if (bene->getUnitsReserve() <= 0) {
		return;
	}

	bool shipAdvisor = true;
	if (ctx.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug("[Bene Gesserit] Spiritual Advisors: ship 1 advisor to " +
				destinationTerritory + " for free? (y/n): ");
		}
		while (true) {
			std::string input;
			std::getline(std::cin >> std::ws, input);
			std::string lowered;
			lowered.reserve(input.size());
			for (char c : input) lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			if (lowered == "y" || lowered == "yes") {
				shipAdvisor = true;
				break;
			}
			if (lowered == "n" || lowered == "no") {
				shipAdvisor = false;
				break;
			}
			if (ctx.logger) {
				ctx.logger->logDebug("Invalid input. Enter y or n: ");
			}
		}
	}

	if (!shipAdvisor) return;

	if (!ctx.map.canAddAdvisorToTerritory(destinationTerritory, beneIndex)) {
		return;
	}

	ctx.map.addUnitsToTerritory(destinationTerritory, beneIndex, 1, 0, -1, true);
	bene->deployUnits(1);
	lockedAdvisorTerritories.insert(destinationTerritory);
	lockedTurn = ctx.turnNumber;

	if (ctx.logger) {
		Event e(EventType::UNITS_SHIPPED,
			"[Bene Gesserit] Spiritual Advisors ships 1 advisor to " + destinationTerritory,
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = "Bene Gesserit";
		e.territory = destinationTerritory;
		e.unitCount = 1;
		e.spiceValue = 0;
		ctx.logger->logEvent(e);
	}
}

bool BeneGesseritAbility::hasPrediction() const {
	return predictionSet;
}

bool BeneGesseritAbility::predictionMatches(const std::string& winningFaction, int turnNumber) const {
	return predictionSet && predictedFaction == winningFaction && predictedTurn == turnNumber;
}

const std::string& BeneGesseritAbility::getPredictedFaction() const {
	return predictedFaction;
}

int BeneGesseritAbility::getPredictedTurn() const {
	return predictedTurn;
}

void BeneGesseritAbility::choosePrediction(PhaseContext& ctx, int beneIndex) {
	if (predictionSet) return;

	std::vector<int> candidates;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (static_cast<int>(i) == beneIndex) continue;
		candidates.push_back(static_cast<int>(i));
	}
	if (candidates.empty()) return;

	if (!ctx.interactiveMode) {
		int pick = candidates[0];
		predictedFaction = ctx.players[pick]->getFactionName();
		predictedTurn = std::min(5, MAX_TURNS);
		predictionSet = true;
		if (ctx.logger) {
			ctx.logger->logDebug("[Bene Gesserit] A secret prediction has been set.");
		}
		return;
	}

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] Secret Prediction setup");
		ctx.logger->logDebug("Choose the faction you predict will win:");
		for (size_t i = 0; i < candidates.size(); ++i) {
			ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + ctx.players[candidates[i]]->getFactionName());
		}
		ctx.logger->logDebug("Choose (1-" + std::to_string(candidates.size()) + "): ");
	}

	int factionPick = -1;
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		try {
			int idx = std::stoi(input);
			if (idx >= 1 && idx <= static_cast<int>(candidates.size())) {
				factionPick = candidates[static_cast<size_t>(idx - 1)];
				break;
			}
		} catch (...) {
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Invalid choice. Try again: ");
		}
	}

	if (ctx.logger) {
		ctx.logger->logDebug("Choose the turn number to predict (1-" + std::to_string(MAX_TURNS) + "): ");
	}

	int turnPick = 1;
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		try {
			int t = std::stoi(input);
			if (t >= 1 && t <= MAX_TURNS) {
				turnPick = t;
				break;
			}
		} catch (...) {
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Invalid turn. Try again: ");
		}
	}

	predictedFaction = ctx.players[factionPick]->getFactionName();
	predictedTurn = turnPick;
	predictionSet = true;

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] A secret prediction has been set.");
	}
}
