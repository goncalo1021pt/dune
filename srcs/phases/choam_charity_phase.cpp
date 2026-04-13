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
		const bool advancedAlwaysCharity =
			ctx.featureSettings.advancedFactionAbilities && ability && ability->alwaysReceivesCharity();

		int charityAmount = 0;
		if (advancedAlwaysCharity) {
			charityAmount = 2;
		} else if (currentSpice <= 1) {
			charityAmount = 2 - currentSpice;
		}

		if (charityAmount > 0) {
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
