#include "phases/spice_blow_phase.hpp"
#include "game.hpp"
#include "map.hpp"
#include <iostream>
#include <algorithm>

spiceCard SpiceBlowPhase::drawSpiceCard(std::vector<spiceCard>& spiceDeck, size_t& spiceDeckIndex,
                                        std::vector<spiceCard>& spiceDiscardPileA,
                                        std::vector<spiceCard>& spiceDiscardPileB,
                                        std::mt19937& rng) {
	if (spiceDeckIndex >= spiceDeck.size()) {
		if (spiceDiscardPileA.empty() && spiceDiscardPileB.empty()) {
			spiceCard fallback;
			fallback.type = spiceCardType::WORM;
			fallback.territoryName = "";
			fallback.spiceAmount = 0;
			return fallback;
		}

		spiceDeck.clear();
		spiceDeck.insert(spiceDeck.end(), spiceDiscardPileA.begin(), spiceDiscardPileA.end());
		spiceDeck.insert(spiceDeck.end(), spiceDiscardPileB.begin(), spiceDiscardPileB.end());
		spiceDiscardPileA.clear();
		spiceDiscardPileB.clear();
		std::shuffle(spiceDeck.begin(), spiceDeck.end(), rng);
		spiceDeckIndex = 0;
		std::cout << "    Spice deck reshuffled from discard piles" << std::endl;
	}

	spiceCard card = spiceDeck[spiceDeckIndex];
	spiceDeckIndex++;
	return card;
}

void SpiceBlowPhase::discardSpiceCard(const spiceCard& card, int discardPileIndex,
                                      bool useExtended,
                                      std::vector<spiceCard>& spiceDiscardPileA,
                                      std::vector<spiceCard>& spiceDiscardPileB) {
	if (useExtended && discardPileIndex == 1) {
		spiceDiscardPileB.push_back(card);
		return;
	}
	spiceDiscardPileA.push_back(card);
}

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

	const int blowCount = ctx.useExtendedSpiceBlow ? 2 : 1;

	for (int blowIndex = 0; blowIndex < blowCount; ++blowIndex) {
		spiceCard card = drawSpiceCard(ctx.spiceDeck, ctx.spiceDeckIndex,
		                               ctx.spiceDiscardPileA, ctx.spiceDiscardPileB,
		                               ctx.rng);
		const int discardPileIndex = ctx.useExtendedSpiceBlow ? blowIndex : 0;
		const std::vector<spiceCard>& targetDiscardPile =
			(ctx.useExtendedSpiceBlow && discardPileIndex == 1) ? ctx.spiceDiscardPileB : ctx.spiceDiscardPileA;

		if (card.type == spiceCardType::WORM) {
			std::cout << "    Draw " << (blowIndex + 1) << ": WORM" << std::endl;

			if (ctx.turnNumber > 1) {
				std::cout << "    NEXUS triggered (alliances not implemented yet)" << std::endl;
			}

			if (targetDiscardPile.empty()) {
				std::cout << "    Worm discarded (no prior discarded card in this pile)" << std::endl;
			} else {
				const spiceCard& topDiscardCard = targetDiscardPile.back();
				if (topDiscardCard.type != spiceCardType::LOCATION) {
					std::cout << "    Worm resolves with no effect (top discard card is WORM)" << std::endl;
				} else {
					territory* targetTerritory = ctx.map.getTerritory(topDiscardCard.territoryName);
					if (targetTerritory == nullptr) {
						std::cout << "    Worm resolves with no effect (invalid target territory)" << std::endl;
					} else if (targetTerritory->spiceAmount <= 0) {
						std::cout << "    Worm resolves on " << topDiscardCard.territoryName
						          << " with no effect (no spice present)" << std::endl;
					} else {
						resolveWormOnTerritory(topDiscardCard.territoryName, ctx.map);
					}
				}
			}

			discardSpiceCard(card, discardPileIndex, ctx.useExtendedSpiceBlow,
			                ctx.spiceDiscardPileA, ctx.spiceDiscardPileB);
			continue;
		}

		territory* terr = ctx.map.getTerritory(card.territoryName);
		if (terr == nullptr) {
			std::cout << "    Draw " << (blowIndex + 1)
			          << ": invalid territory card discarded" << std::endl;
			discardSpiceCard(card, discardPileIndex, ctx.useExtendedSpiceBlow,
			                ctx.spiceDiscardPileA, ctx.spiceDiscardPileB);
			continue;
		}

		terr->spiceAmount += card.spiceAmount;

		std::cout << "    Draw " << (blowIndex + 1) << ": "
		          << card.territoryName << " (" << card.spiceAmount << " spice)"
		          << " -> territory now has " << terr->spiceAmount << std::endl;

		discardSpiceCard(card, discardPileIndex, ctx.useExtendedSpiceBlow,
		                ctx.spiceDiscardPileA, ctx.spiceDiscardPileB);
	}
}
