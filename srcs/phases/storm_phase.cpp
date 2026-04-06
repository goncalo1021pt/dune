#include "phases/storm_phase.hpp"
#include <algorithm>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

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
	while (stormSector > TOTAL_SECTORS) {
		stormSector -= TOTAL_SECTORS;
	}
	while (stormSector < 1) {
		stormSector += TOTAL_SECTORS;
	}
}

void StormPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("STORM Phase");
	}
	auto view = ctx.getStormView();

	if (view.turnNumber == 1) {
		std::uniform_int_distribution<> startSectorDist(1, 18);
		view.stormSector = startSectorDist(view.rng);
		view.lastStormCard = 0;
		view.nextStormCard = drawStormCard(view.stormDeck, view.rng);
		view.hasNextStormCard = true;

		if (ctx.logger) {
			Event e(EventType::STORM_PLACED,
				"storm placed at random sector " + std::to_string(view.stormSector),
				ctx.turnNumber, "STORM");
			e.territory = std::to_string(view.stormSector);
			ctx.logger->logEvent(e);
		}
		return;
	}

	if (!view.hasNextStormCard) {
		view.nextStormCard = drawStormCard(view.stormDeck, view.rng);
		view.hasNextStormCard = true;
	}

	view.lastStormCard = view.nextStormCard;
	view.hasNextStormCard = false;
	moveStorm(view.lastStormCard, view.stormSector);

	view.nextStormCard = drawStormCard(view.stormDeck, view.rng);
	view.hasNextStormCard = true;

	if (ctx.logger) {
		Event e(EventType::STORM_MOVED,
			"storm moved: card " + std::to_string(view.lastStormCard) + 
			" -> now at sector " + std::to_string(view.stormSector),
			ctx.turnNumber, "STORM");
		e.spiceValue = view.lastStormCard;
		ctx.logger->logEvent(e);
	}
}
