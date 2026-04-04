#include "phases/simple_phases.hpp"
#include "game.hpp"
#include "player.hpp"
#include <iostream>
#include <algorithm>

void ChoamCharityPhase::execute(PhaseContext& ctx) {
	std::cout << "  CHOAM_CHARITY Phase" << std::endl;
	for (int i = 0; i < ctx.playerCount; ++i) {
		int currentSpice = ctx.players[i]->getSpice();
		bool isBeneGesserit = (ctx.players[i]->getFactionIndex() == static_cast<int>(faction::BENE_GESSERIT));

		bool shouldReceiveCharity = (currentSpice <= 1);
		if (ctx.beneGesseritCharity && isBeneGesserit) {
			shouldReceiveCharity = true;
		}

		if (shouldReceiveCharity && currentSpice < 2) {
			int charityAmount = 2 - currentSpice;
			ctx.players[i]->addSpice(charityAmount);
			std::cout << "    " << ctx.players[i]->getFactionName() << " receives "
			          << charityAmount << " spice from CHOAM (now at "
			          << ctx.players[i]->getSpice() << ")" << std::endl;
		}
	}
}

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
