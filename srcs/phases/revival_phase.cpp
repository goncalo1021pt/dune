#include "phases/revival_phase.hpp"
#include "player.hpp"
#include <algorithm>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

void RevivalPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("REVIVAL Phase");
	}
	auto view = ctx.getRevivalView();
	const int spiceCostPerPaidRevive = 2;

	for (size_t i = 0; i < view.players.size(); ++i) {
		int destroyed = view.players[i]->getUnitsDestroyed();
		if (destroyed <= 0) {
			continue;
		}

		FactionAbility* ability = ctx.getAbility(i);
		int freeRevives = (ability) ? ability->getFreeRevivalsPerTurn() : 1;
		int revivedForFree = std::min(freeRevives, destroyed);

		int revivedPaid = 0;
		if (ability && ability->canBuyAdditionalRevivals()) {
			int destroyedAfterFree = destroyed - revivedForFree;
			int affordablePaidRevives = view.players[i]->getSpice() / spiceCostPerPaidRevive;
			revivedPaid = std::min(destroyedAfterFree, affordablePaidRevives);
		}
		int spicePaid = revivedPaid * spiceCostPerPaidRevive;
		int totalRevived = revivedForFree + revivedPaid;

		if (totalRevived <= 0) {
			continue;
		}

		if (spicePaid > 0) {
			view.players[i]->removeSpice(spicePaid);
		}
		view.players[i]->reviveUnits(totalRevived);

		if (ctx.logger) {
			std::string msg = "revives " + std::to_string(totalRevived) + " units (" + std::to_string(revivedForFree) + " free";
			if (revivedPaid > 0) {
				msg += ", " + std::to_string(revivedPaid) + " paid for " + std::to_string(spicePaid) + " spice";
			}
			msg += ")";
			
			Event e(EventType::LEADER_REVIVED,
				msg,
				ctx.turnNumber, "REVIVAL");
			e.playerFaction = view.players[i]->getFactionName();
			e.unitCount = totalRevived;
			e.spiceValue = spicePaid;
			ctx.logger->logEvent(e);
		}
	}
}
