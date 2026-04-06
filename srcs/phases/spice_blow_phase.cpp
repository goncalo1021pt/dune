#include "phases/spice_blow_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include <iostream>
#include <algorithm>

void SpiceBlowPhase::resolveWormOnTerritory(const std::string& territoryName, GameMap& map) {
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

	std::cout << "    Worm devours " << territoryName << ": "
	          << totalUnitsKilled << " units and "
	          << spiceDestroyed << " spice removed" << std::endl;
}

void SpiceBlowPhase::execute(PhaseContext& ctx) {
	std::cout << "  SPICE_BLOW Phase" << std::endl;

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
			std::cout << "    Draw " << (blowIndex + 1) << ": WORM" << std::endl;

			if (view.turnNumber > 1) {
				std::cout << "    NEXUS triggered (alliances not implemented yet)" << std::endl;
			}

			if (targetDiscardPile.empty()) {
				std::cout << "    Worm discarded (no prior discarded card in this pile)" << std::endl;
			} else {
				const spiceCard& topDiscardCard = targetDiscardPile.back();
				if (topDiscardCard.type != spiceCardType::LOCATION) {
					std::cout << "    Worm resolves with no effect (top discard card is WORM)" << std::endl;
				} else {
					territory* targetTerritory = view.map.getTerritory(topDiscardCard.territoryName);
					if (targetTerritory == nullptr) {
						std::cout << "    Worm resolves with no effect (invalid target territory)" << std::endl;
					} else if (targetTerritory->spiceAmount <= 0) {
						std::cout << "    Worm resolves on " << topDiscardCard.territoryName
						          << " with no effect (no spice present)" << std::endl;
					} else {
						resolveWormOnTerritory(topDiscardCard.territoryName, view.map);
					}
				}
			}

			view.spiceDeck.discardCard(card, discardPileIndex);
			continue;
		}

		territory* terr = view.map.getTerritory(card.territoryName);
		if (terr == nullptr) {
			std::cout << "    Draw " << (blowIndex + 1)
			          << ": invalid territory card discarded" << std::endl;
			view.spiceDeck.discardCard(card, discardPileIndex);
			continue;
		}

		terr->spiceAmount += card.spiceAmount;

		std::cout << "    Draw " << (blowIndex + 1) << ": "
		          << card.territoryName << " (" << card.spiceAmount << " spice)"
		          << " -> territory now has " << terr->spiceAmount << std::endl;

		view.spiceDeck.discardCard(card, discardPileIndex);
	}
}
