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
		int destroyedNormal = view.players[i]->getUnitsDestroyed();
		int destroyedElite = view.players[i]->getEliteUnitsDestroyed();
		int destroyedTotal = destroyedNormal + destroyedElite;
		if (destroyedTotal <= 0) {
			continue;
		}

		FactionAbility* ability = ctx.getAbility(i);
		int freeRevives = (ability) ? ability->getFreeRevivalsPerTurn() : 1;
		int revivedForFree = std::min(freeRevives, destroyedTotal);

		int revivedNormalFree = std::min(revivedForFree, destroyedNormal);
		int revivedEliteFree = revivedForFree - revivedNormalFree;

		int remainingNormal = destroyedNormal - revivedNormalFree;
		int remainingElite = destroyedElite - revivedEliteFree;

		int revivedPaid = 0;
		if (ability && ability->canBuyAdditionalRevivals()) {
			int destroyedAfterFree = remainingNormal + remainingElite;
			int affordablePaidRevives = view.players[i]->getSpice() / spiceCostPerPaidRevive;
			revivedPaid = std::min(destroyedAfterFree, affordablePaidRevives);
		}

		int revivedNormalPaid = std::min(revivedPaid, remainingNormal);
		int revivedElitePaid = revivedPaid - revivedNormalPaid;

		int revivedNormalTotal = revivedNormalFree + revivedNormalPaid;
		int revivedEliteTotal = revivedEliteFree + revivedElitePaid;
		int totalRevived = revivedNormalTotal + revivedEliteTotal;

		int spicePaid = revivedPaid * spiceCostPerPaidRevive;

		if (totalRevived <= 0) {
			continue;
		}

		if (spicePaid > 0) {
			view.players[i]->removeSpice(spicePaid);
		}
		if (revivedNormalTotal > 0) {
			view.players[i]->reviveUnits(revivedNormalTotal);
		}
		if (revivedEliteTotal > 0) {
			view.players[i]->reviveEliteUnits(revivedEliteTotal);
		}

		if (ctx.logger) {
			std::string msg = "revives " + std::to_string(totalRevived) + " units ("
				+ std::to_string(revivedNormalTotal) + " normal, "
				+ std::to_string(revivedEliteTotal) + " elite; "
				+ std::to_string(revivedForFree) + " free";
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
