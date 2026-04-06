#include "phases/choam_charity_phase.hpp"
#include "player.hpp"
#include <iostream>

void ChoamCharityPhase::execute(PhaseContext& ctx) {
	std::cout << "  CHOAM_CHARITY Phase" << std::endl;
	auto view = ctx.getChoamCharityView();
	for (size_t i = 0; i < view.players.size(); ++i) {
		int currentSpice = view.players[i]->getSpice();
		bool isBeneGesserit = (view.players[i]->getFactionIndex() == 2);  // Bene Gesserit index

		bool shouldReceiveCharity = (currentSpice <= 1);
		if (view.beneGesseritCharity && isBeneGesserit) {
			shouldReceiveCharity = true;
		}

		if (shouldReceiveCharity && currentSpice < 2) {
			int charityAmount = 2 - currentSpice;
			view.players[i]->addSpice(charityAmount);
			std::cout << "    " << view.players[i]->getFactionName() << " receives "
			          << charityAmount << " spice from CHOAM (now at "
			          << view.players[i]->getSpice() << ")" << std::endl;
		}
	}
}
