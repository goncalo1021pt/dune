#include "phases/choam_charity_phase.hpp"
#include "player.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"

void ChoamCharityPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("CHOAM_CHARITY Phase");
	}
	auto view = ctx.getChoamCharityView();
	for (size_t i = 0; i < view.players.size(); ++i) {
		int currentSpice = view.players[i]->getSpice();
		FactionAbility* ability = ctx.getAbility(i);

		bool shouldReceiveCharity = (currentSpice <= 1);
		if (ability && ability->alwaysReceivesCharity()) {
			shouldReceiveCharity = true;
		}

		if (shouldReceiveCharity && currentSpice < 2) {
			int charityAmount = 2 - currentSpice;
			view.players[i]->addSpice(charityAmount);
			
			if (ctx.logger) {
				Event e(EventType::SPICE_CHARITY, 
					std::string("receives ") + std::to_string(charityAmount) + " spice from CHOAM",
					ctx.turnNumber, "CHOAM_CHARITY");
				e.playerFaction = view.players[i]->getFactionName();
				e.spiceValue = charityAmount;
				ctx.logger->logEvent(e);
			}
		}
	}
}
