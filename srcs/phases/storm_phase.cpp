#include "phases/storm_phase.hpp"
#include <iostream>
#include <algorithm>

void StormPhase::initializeStormDeck(std::vector<int>& stormDeck, std::mt19937& rng) {
	stormDeck = {1, 2, 3, 4, 5, 6};
	std::shuffle(stormDeck.begin(), stormDeck.end(), rng);
}

int StormPhase::drawStormCard(std::vector<int>& stormDeck, std::mt19937& rng) {
	// Draw first card from deck
	int card = stormDeck[0];
	// Reshuffle deck for next turn (allows same card to be drawn consecutively)
	initializeStormDeck(stormDeck, rng);
	return card;
}

void StormPhase::moveStorm(int sectorsToMove, int& stormSector) {
	stormSector += sectorsToMove;
	while (stormSector > 18) {
		stormSector -= 18;
	}
	while (stormSector < 1) {
		stormSector += 18;
	}
}

void StormPhase::execute(PhaseContext& ctx) {
	std::cout << "  STORM Phase" << std::endl;

	if (ctx.turnNumber == 1) {
		std::uniform_int_distribution<> startSectorDist(1, 18);
		ctx.stormSector = startSectorDist(ctx.rng);
		ctx.lastStormCard = 0;
		ctx.nextStormCard = drawStormCard(ctx.stormDeck, ctx.rng);
		ctx.hasNextStormCard = true;

		std::cout << "    First turn setup: storm placed at random sector " << ctx.stormSector << std::endl;
		std::cout << "    Next storm card prepared: " << ctx.nextStormCard << std::endl;
		return;
	}

	if (!ctx.hasNextStormCard) {
		ctx.nextStormCard = drawStormCard(ctx.stormDeck, ctx.rng);
		ctx.hasNextStormCard = true;
	}

	ctx.lastStormCard = ctx.nextStormCard;
	ctx.hasNextStormCard = false;
	moveStorm(ctx.lastStormCard, ctx.stormSector);

	ctx.nextStormCard = drawStormCard(ctx.stormDeck, ctx.rng);
	ctx.hasNextStormCard = true;

	std::cout << "    Storm card resolved: " << ctx.lastStormCard << std::endl;
	std::cout << "    Storm now at sector " << ctx.stormSector << std::endl;
	std::cout << "    Next storm card prepared: " << ctx.nextStormCard << std::endl;
}
