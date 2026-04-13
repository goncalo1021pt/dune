#include "phases/mentat_pause_phase.hpp"
#include "phases/phase_context.hpp"
#include "map.hpp"
#include "player.hpp"
#include "factions/bene_gesserit_ability.hpp"
#include "logger/event_logger.hpp"
#include "events/event.hpp"

int MentatPausePhase::countCitiesControlled(PhaseContext& ctx, int factionIndex) {
	auto view = ctx.getMentatPauseView();
	int citiesControlled = 0;

	// Count all territories that are cities and controlled by this faction
	for (const auto& territory : view.map.getTerritories()) {
		if (territory.terrain == terrainType::city && view.map.isControlled(territory.name, factionIndex)) {
			citiesControlled++;
		}
	}

	return citiesControlled;
}

bool MentatPausePhase::checkVictory(PhaseContext& ctx) {
	auto view = ctx.getMentatPauseView();

	// Check each player for victory condition
	for (size_t i = 0; i < view.players.size(); ++i) {
		int citiesControlled = countCitiesControlled(ctx, i);

		if (citiesControlled >= 3) {
			int beneIndex = -1;
			BeneGesseritAbility* beneAbility = nullptr;
			for (size_t j = 0; j < view.players.size(); ++j) {
				FactionAbility* ability = view.players[j]->getFactionAbility();
				auto* bg = dynamic_cast<BeneGesseritAbility*>(ability);
				if (bg) {
					beneIndex = static_cast<int>(j);
					beneAbility = bg;
					break;
				}
			}

			if (beneAbility && beneAbility->predictionMatches(view.players[i]->getFactionName(), ctx.turnNumber)) {
				if (ctx.logger) {
					ctx.logger->logDebug("");
					ctx.logger->logDebug("╔════════════════════════════════════════╗");
					ctx.logger->logDebug("║      BENE GESSERIT PREDICTION HIT!     ║");
					ctx.logger->logDebug("╚════════════════════════════════════════╝");

					std::string victoryMsg = "Bene Gesserit reveals prediction (" +
						beneAbility->getPredictedFaction() + " on turn " +
						std::to_string(beneAbility->getPredictedTurn()) +
						") and wins alone!";
					ctx.logger->logDebug(victoryMsg);

					Event e(EventType::GAME_ENDED,
						victoryMsg,
						ctx.turnNumber, "MENTAT_PAUSE");
					e.playerFaction = (beneIndex >= 0) ? view.players[beneIndex]->getFactionName() : "Bene Gesserit";
					ctx.logger->logEvent(e);
				}
				return true;
			}

			if (ctx.logger) {
				ctx.logger->logDebug("");
				ctx.logger->logDebug("╔════════════════════════════════════════╗");
				ctx.logger->logDebug("║              GAME VICTORY!             ║");
				ctx.logger->logDebug("╚════════════════════════════════════════╝");

				std::string victoryMsg = view.players[i]->getFactionName() + " controls " + 
					std::to_string(citiesControlled) + " cities and wins!";
				ctx.logger->logDebug(victoryMsg);

				Event e(EventType::GAME_ENDED,
					victoryMsg,
					ctx.turnNumber, "MENTAT_PAUSE");
				e.playerFaction = view.players[i]->getFactionName();
				ctx.logger->logEvent(e);
			}
			return true;
		}
	}

	return false;
}

void MentatPausePhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("MENTAT_PAUSE Phase");
	}

	auto view = ctx.getMentatPauseView();

	// Report city control status
	if (ctx.logger) {
		ctx.logger->logDebug("City Control Status:");
	}
	for (size_t i = 0; i < view.players.size(); ++i) {
		int citiesControlled = countCitiesControlled(ctx, i);
		if (ctx.logger) {
			std::string statusMsg = "  " + view.players[i]->getFactionName() + ": " + 
				std::to_string(citiesControlled) + "/3 cities";
			ctx.logger->logDebug(statusMsg);
		}
	}

	// Check for victory
	if (checkVictory(ctx)) {
		ctx.gameEnded = true;
	}
}
