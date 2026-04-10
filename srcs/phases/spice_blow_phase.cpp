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

	// Check if any faction can ride the worm (Fremen only, currently)
	bool anyoneRode = false;
	if (ctxPtr) {
		for (int i = 0; i < ctxPtr->playerCount; ++i) {
			if (ctxPtr->players[i]->getFactionAbility()->canRideWorm()) {
				if (ctxPtr->players[i]->getFactionAbility()->onWormHitsTerritory(*ctxPtr, territoryName)) {
					anyoneRode = true;
					break;
				}
			}
		}
	}

	int totalUnitsKilled = 0;
	for (const auto& stack : terr->unitsPresent) {
		totalUnitsKilled += stack.normal_units + stack.elite_units;
	}

	terr->unitsPresent.clear();
	int spiceDestroyed = map.getSpiceInTerritory(territoryName);
	map.removeAllSpiceFromTerritory(territoryName);

	if (ctxPtr && ctxPtr->logger) {
		if (anyoneRode) {
			Event e(EventType::UNITS_KILLED,
				"Worm at " + territoryName + ": Fremen rode away (no casualties)",
				ctxPtr->turnNumber, "SPICE_BLOW");
			e.territory = territoryName;
			e.unitCount = 0;
			e.spiceValue = spiceDestroyed;
			ctxPtr->logger->logEvent(e);
		} else {
			Event e(EventType::UNITS_KILLED,
				"Worm at " + territoryName + ": devours " + std::to_string(totalUnitsKilled) + " units, " + 
				std::to_string(spiceDestroyed) + " spice destroyed",
				ctxPtr->turnNumber, "SPICE_BLOW");
			e.territory = territoryName;
			e.unitCount = totalUnitsKilled;
			e.spiceValue = spiceDestroyed;
			ctxPtr->logger->logEvent(e);
		}
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
					} else {
						// Worm resolves REGARDLESS of spice - it kills units even on barren territories
						if (ctx.logger) {
							ctx.logger->logDebug("Worm strikes: " + topDiscardCard.territoryName);
						}
						resolveWormOnTerritory(topDiscardCard.territoryName, view.map, &ctx);
						
						// Draw an additional card after worm resolves
						spiceCard extraCard = view.spiceDeck.drawCard();
						if (extraCard.type == spiceCardType::LOCATION) {
							territory* extraTerr = view.map.getTerritory(extraCard.territoryName);
							if (extraTerr != nullptr) {
								int sector = extraTerr->sectors.empty() ? -1 : extraTerr->sectors[0];
								view.map.addSpiceToTerritory(extraCard.territoryName, extraCard.spiceAmount, sector);
								if (ctx.logger) {
									Event e(EventType::SPICE_BLOWN,
										"[After Worm] " + std::to_string(extraCard.spiceAmount) + " spice placed at " + extraCard.territoryName,
										ctx.turnNumber, "SPICE_BLOW");
									e.territory = extraCard.territoryName;
									e.spiceValue = extraCard.spiceAmount;
									ctx.logger->logEvent(e);
								}
							}
						}
						view.spiceDeck.discardCard(extraCard, discardPileIndex);
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

		int sector = terr->sectors.empty() ? -1 : terr->sectors[0];
		view.map.addSpiceToTerritory(card.territoryName, card.spiceAmount, sector);

		if (ctx.logger) {
			Event e(EventType::SPICE_BLOWN,
				"Draw " + std::to_string(blowIndex + 1) + ": " + 
				std::to_string(card.spiceAmount) + " spice placed at " + card.territoryName,
				ctx.turnNumber, "SPICE_BLOW");
			e.territory = card.territoryName;
			e.spiceValue = card.spiceAmount;
			ctx.logger->logEvent(e);
		}

		view.spiceDeck.discardCard(card, discardPileIndex);
	}

	// Atreides prescience: after this phase resolves, peek what will blow next turn.
	if (ctx.featureSettings.atreidesPeekNextSpiceBlowCards && ctx.logger) {
		bool hasAtreides = false;
		for (int i = 0; i < ctx.playerCount; ++i) {
			FactionAbility* ability = ctx.players[i]->getFactionAbility();
			if (ability && ability->getFactionName() == "Atreides") {
				hasAtreides = true;
				break;
			}
		}

		if (hasAtreides) {
			auto upcoming = view.spiceDeck.peekNextCards(blowCount);
			for (size_t i = 0; i < upcoming.size(); ++i) {
				const spiceCard& c = upcoming[i];
				if (c.type == spiceCardType::WORM) {
					ctx.logger->logDebug("[Atreides Prescience] Next turn Spice Blow card " + std::to_string(i + 1) + ": WORM");
				} else {
					ctx.logger->logDebug("[Atreides Prescience] Next turn Spice Blow card " + std::to_string(i + 1) + ": " +
						c.territoryName + " (" + std::to_string(c.spiceAmount) + " spice)");
				}
			}
		}
	}
}
