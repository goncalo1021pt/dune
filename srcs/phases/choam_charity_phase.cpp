#include "phases/choam_charity_phase.hpp"
#include "player.hpp"
#include <iostream>

void ChoamCharityPhase::execute(PhaseContext& ctx) {
	std::cout << "  CHOAM_CHARITY Phase" << std::endl;
	for (int i = 0; i < ctx.playerCount; ++i) {
		int currentSpice = ctx.players[i]->getSpice();
		bool isBeneGesserit = (ctx.players[i]->getFactionIndex() == 2);  // Bene Gesserit index

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
