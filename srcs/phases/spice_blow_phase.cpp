#include "phases/spice_blow_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include <algorithm>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

void SpiceBlowPhase::resolveWormOnTerritory(const std::string& territoryName, GameMap& map, PhaseContext* ctxPtr) {
	territory* terr = map.getTerritory(territoryName);
	if (terr == nullptr) {
		return;
	}

	int totalUnitsKilled = 0;
	for (const auto& stack : terr->unitsPresent) {
		totalUnitsKilled += stack.normal_units + stack.elite_units;
	}

	terr->unitsPresent.clear();
	int spiceDestroyed = terr->spiceAmount;
	terr->spiceAmount = 0;

	if (ctxPtr && ctxPtr->logger) {
		Event e(EventType::UNITS_KILLED,
			"Worm devours: " + std::to_string(totalUnitsKilled) + " units and " + 
			std::to_string(spiceDestroyed) + " spice removed",
			ctxPtr->turnNumber, "SPICE_BLOW");
		e.territory = territoryName;
		e.unitCount = totalUnitsKilled;
		e.spiceValue = spiceDestroyed;
		ctxPtr->logger->logEvent(e);
	}
}

void SpiceBlowPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("SPICE_BLOW Phase");
	}

	// Get minimal view (only what this phase needs)
	auto view = ctx.getSpiceBlowView();

	const int blowCount = view.spiceDeck.isUsingExtendedSpiceBlow() ? 2 : 1;

	for (int blowIndex = 0; blowIndex < blowCount; ++blowIndex) {
		spiceCard card = view.spiceDeck.drawCard();
		const int discardPileIndex = view.spiceDeck.isUsingExtendedSpiceBlow() ? blowIndex : 0;
		const std::vector<spiceCard>& targetDiscardPile =
			(view.spiceDeck.isUsingExtendedSpiceBlow() && discardPileIndex == 1) 
				? view.spiceDeck.getDiscardPileB() 
				: view.spiceDeck.getDiscardPileA();

		if (card.type == spiceCardType::WORM) {
			if (ctx.logger) {
				ctx.logger->logDebug("Draw " + std::to_string(blowIndex + 1) + ": WORM");
			}

			if (view.turnNumber > 1 && ctx.logger) {
				ctx.logger->logDebug("NEXUS triggered (alliances not implemented yet)");
			}

			if (targetDiscardPile.empty()) {
				if (ctx.logger) {
					ctx.logger->logDebug("Worm discarded (no prior discarded card)");
				}
			} else {
				const spiceCard& topDiscardCard = targetDiscardPile.back();
				if (topDiscardCard.type != spiceCardType::LOCATION) {
					if (ctx.logger) {
						ctx.logger->logDebug("Worm with no effect (top card is WORM)");
					}
				} else {
					territory* targetTerritory = view.map.getTerritory(topDiscardCard.territoryName);
					if (targetTerritory == nullptr) {
						if (ctx.logger) {
							ctx.logger->logDebug("Worm with no effect (invalid territory)");
						}
					} else if (targetTerritory->spiceAmount <= 0) {
						if (ctx.logger) {
							ctx.logger->logDebug("Worm on " + topDiscardCard.territoryName + " (no spice)");
						}
					} else {
						resolveWormOnTerritory(topDiscardCard.territoryName, view.map, &ctx);
					}
				}
			}

			view.spiceDeck.discardCard(card, discardPileIndex);
			continue;
		}

		territory* terr = view.map.getTerritory(card.territoryName);
		if (terr == nullptr) {
			if (ctx.logger) {
				ctx.logger->logDebug("Draw " + std::to_string(blowIndex + 1) + ": invalid territory card");
			}
			view.spiceDeck.discardCard(card, discardPileIndex);
			continue;
		}

		terr->spiceAmount += card.spiceAmount;

		if (ctx.logger) {
			Event e(EventType::SPICE_BLOWN,
				"Draw " + std::to_string(blowIndex + 1) + ": " + 
				std::to_string(card.spiceAmount) + " spice placed",
				ctx.turnNumber, "SPICE_BLOW");
			e.territory = card.territoryName;
			e.spiceValue = card.spiceAmount;
			ctx.logger->logEvent(e);
		}

		view.spiceDeck.discardCard(card, discardPileIndex);
	}
}
