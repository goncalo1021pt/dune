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
	if (bene->getUnitsReserve() > 0) {
		ctx.map.addUnitsToTerritory("Polar Sink", beneIndex, 1, 0, -1);
		bene->deployUnits(1);
	}

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Gesserit] Starts with 1 force in Polar Sink, 19 in reserves, and 5 spice");
	}

	choosePrediction(ctx, beneIndex);
}

int BeneGesseritAbility::getFreeRevivalsPerTurn() const {
	return 1;
}

void BeneGesseritAbility::onOtherFactionShipped(PhaseContext& ctx, int shippingFactionIndex, int amount) {
	(void)amount;
	if (shippingFactionIndex < 0 || shippingFactionIndex >= static_cast<int>(ctx.players.size())) {
		return;
	}

	// Rule exception: Spiritual Advisors does not trigger on Fremen shipments.
	FactionAbility* shippingAbility = ctx.players[shippingFactionIndex]->getFactionAbility();
	if (shippingAbility && shippingAbility->getFactionName() == "Fremen") {
		return;
	}

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
			ctx.logger->logDebug("[Bene Gesserit] Spiritual Advisors: ship 1 force to Polar Sink for free? (y/n): ");
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

	ctx.map.addUnitsToTerritory("Polar Sink", beneIndex, 1, 0, -1);
	bene->deployUnits(1);

	if (ctx.logger) {
		Event e(EventType::UNITS_SHIPPED,
			"[Bene Gesserit] Spiritual Advisors ships 1 force to Polar Sink",
			ctx.turnNumber, "SHIP_AND_MOVE");
		e.playerFaction = "Bene Gesserit";
		e.territory = "Polar Sink";
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
