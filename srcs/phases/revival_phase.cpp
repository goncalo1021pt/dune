#include "phases/revival_phase.hpp"
#include "player.hpp"
#include <iostream>
#include <algorithm>

void RevivalPhase::execute(PhaseContext& ctx) {
	std::cout << "  REVIVAL Phase" << std::endl;
	const int maxRevivesPerTurn = 3;
	const int spiceCostPerPaidRevive = 2;

	for (int i = 0; i < ctx.playerCount; ++i) {
		int destroyed = ctx.players[i]->getUnitsDestroyed();
		if (destroyed <= 0) {
			continue;
		}

		int freeRevives = ctx.players[i]->getFreeRevivesPerTurn();
		int revivedForFree = std::min(std::min(freeRevives, maxRevivesPerTurn), destroyed);

		int reviveSlotsLeft = maxRevivesPerTurn - revivedForFree;
		int destroyedAfterFree = destroyed - revivedForFree;
		int affordablePaidRevives = ctx.players[i]->getSpice() / spiceCostPerPaidRevive;
		int revivedPaid = std::min(std::min(reviveSlotsLeft, destroyedAfterFree), affordablePaidRevives);
		int spicePaid = revivedPaid * spiceCostPerPaidRevive;
		int totalRevived = revivedForFree + revivedPaid;

		if (totalRevived <= 0) {
			continue;
		}

		if (spicePaid > 0) {
			ctx.players[i]->removeSpice(spicePaid);
		}
		ctx.players[i]->reviveUnits(totalRevived);

		std::cout << "    " << ctx.players[i]->getFactionName() << " revives "
		          << totalRevived << " units (" << revivedForFree << " free";
		if (revivedPaid > 0) {
			std::cout << ", " << revivedPaid << " paid for " << spicePaid << " spice";
		}
		std::cout << ")" << std::endl;
	}
}
