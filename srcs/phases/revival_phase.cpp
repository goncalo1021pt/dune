#include "phases/revival_phase.hpp"
#include "player.hpp"
#include <iostream>
#include <algorithm>

void RevivalPhase::execute(PhaseContext& ctx) {
	std::cout << "  REVIVAL Phase" << std::endl;
	auto view = ctx.getRevivalView();
	const int maxRevivesPerTurn = 3;
	const int spiceCostPerPaidRevive = 2;

	for (size_t i = 0; i < view.players.size(); ++i) {
		int destroyed = view.players[i]->getUnitsDestroyed();
		if (destroyed <= 0) {
			continue;
		}

		int freeRevives = view.players[i]->getFreeRevivesPerTurn();
		int revivedForFree = std::min(std::min(freeRevives, maxRevivesPerTurn), destroyed);

		int reviveSlotsLeft = maxRevivesPerTurn - revivedForFree;
		int destroyedAfterFree = destroyed - revivedForFree;
		int affordablePaidRevives = view.players[i]->getSpice() / spiceCostPerPaidRevive;
		int revivedPaid = std::min(std::min(reviveSlotsLeft, destroyedAfterFree), affordablePaidRevives);
		int spicePaid = revivedPaid * spiceCostPerPaidRevive;
		int totalRevived = revivedForFree + revivedPaid;

		if (totalRevived <= 0) {
			continue;
		}

		if (spicePaid > 0) {
			view.players[i]->removeSpice(spicePaid);
		}
		view.players[i]->reviveUnits(totalRevived);

		std::cout << "    " << view.players[i]->getFactionName() << " revives "
		          << totalRevived << " units (" << revivedForFree << " free";
		if (revivedPaid > 0) {
			std::cout << ", " << revivedPaid << " paid for " << spicePaid << " spice";
		}
		std::cout << ")" << std::endl;
	}
}
