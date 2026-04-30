#include "reactions/reaction_engine.hpp"
#include "phases/phase_context.hpp"
#include "player.hpp"
#include "factions/faction_ability.hpp"
#include "interaction/interaction_adapter.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"

#include <algorithm>
#include <random>

namespace {

bool playerHasCard(const Player* p, const std::string& name) {
	const auto& cards = p->getTreacheryCards();
	return std::find(cards.begin(), cards.end(), name) != cards.end();
}

// Static mapping from reaction-card name to the window in which it is legal.
// Keep alphabetised. Cards not listed are treated as not-a-reaction (always
// illegal via the engine; ordinary play during BIDDING/BATTLE is unaffected
// since those plays don't go through ReactionEngine::isReactionLegalNow).
ReactionWindow legalWindowForCard(const std::string& cardName) {
	if (cardName == "Weather Control") return ReactionWindow::BeforeStormMove;
	if (cardName == "Hajr")            return ReactionWindow::AfterMovement;
	if (cardName == "Tleilaxu Ghola")  return ReactionWindow::AnytimeSafe;
	return ReactionWindow::None;
}

} // namespace

const char* reactionWindowName(ReactionWindow w) {
	switch (w) {
		case ReactionWindow::BeforeStormMove:        return "BeforeStormMove";
		case ReactionWindow::AfterStormMove:         return "AfterStormMove";
		case ReactionWindow::BeforeShipment:         return "BeforeShipment";
		case ReactionWindow::AfterShipment:          return "AfterShipment";
		case ReactionWindow::BeforeMovement:         return "BeforeMovement";
		case ReactionWindow::AfterMovement:          return "AfterMovement";
		case ReactionWindow::BeforeBattlePlanReveal: return "BeforeBattlePlanReveal";
		case ReactionWindow::AfterBattlePlanReveal:  return "AfterBattlePlanReveal";
		case ReactionWindow::BeforeBattleResolution: return "BeforeBattleResolution";
		case ReactionWindow::AfterBattleResolution:  return "AfterBattleResolution";
		case ReactionWindow::BeforeRevival:          return "BeforeRevival";
		case ReactionWindow::AfterRevival:           return "AfterRevival";
		case ReactionWindow::AnytimeSafe:            return "AnytimeSafe";
		case ReactionWindow::None:                   return "None";
	}
	return "Unknown";
}

bool ReactionEngine::isReactionLegalNow(const std::string& cardName) const {
	ReactionWindow needed = legalWindowForCard(cardName);
	if (needed == ReactionWindow::None) return false;

	// AnytimeSafe is also satisfied by any other open window (Ghola can be
	// played at any safe moment, including window boundaries that themselves
	// are reaction windows — those are still safe checkpoints by definition).
	if (needed == ReactionWindow::AnytimeSafe) {
		return currentWindow_ != ReactionWindow::None;
	}
	return currentWindow_ == needed;
}

void ReactionEngine::logWindowOpen(PhaseContext& ctx, ReactionWindow w,
	const std::string& detail) const {
	if (!ctx.logger) return;
	std::string msg = "[Reaction] window opened: ";
	msg += reactionWindowName(w);
	if (!detail.empty()) {
		msg += " (";
		msg += detail;
		msg += ")";
	}
	Event e(EventType::REACTION_WINDOW_OPENED, msg, ctx.turnNumber, "");
	ctx.logger->logEvent(e);
}

// --- Storm: BeforeStormMove (Weather Control) ----------------------------

int ReactionEngine::dispatchBeforeStormMove(PhaseContext& ctx, int defaultMove,
	int& weatherControllerOut) {
	weatherControllerOut = -1;
	int chosenMove = defaultMove;

	WindowGuard guard(*this, ReactionWindow::BeforeStormMove);

	for (int idx : ctx.turnOrder) {
		if (!playerHasCard(ctx.players[idx], "Weather Control")) continue;

		bool play = false;
		if (ctx.adapter) {
			DecisionRequest req;
			req.kind = "yn";
			req.actor_index = idx;
			req.prompt = ctx.players[idx]->getFactionName() +
				", play Weather Control to set storm movement (0-10)?";
			auto resp = ctx.adapter->requestDecision(req);
			play = resp && resp->valid && resp->payload_json == "y";
		}
		if (!play) continue;

		// Player accepted — ask for the override value.
		if (ctx.adapter) {
			DecisionRequest req;
			req.kind = "int";
			req.actor_index = idx;
			req.prompt = "Enter storm movement sectors (0-10): ";
			req.int_min = 0;
			req.int_max = 10;
			auto resp = ctx.adapter->requestDecision(req);
			if (resp && resp->valid) {
				try { chosenMove = std::stoi(resp->payload_json); } catch (...) {}
			}
		}

		ctx.players[idx]->removeTreacheryCard("Weather Control");
		weatherControllerOut = idx;
		logWindowOpen(ctx, ReactionWindow::BeforeStormMove,
			ctx.players[idx]->getFactionName() + " played Weather Control");
		break;
	}

	return chosenMove;
}

// --- Movement: AfterMovement (Hajr) --------------------------------------

bool ReactionEngine::dispatchAfterMovementHajr(PhaseContext& ctx,
	int playerIndex) {
	if (playerIndex < 0 || playerIndex >= static_cast<int>(ctx.players.size())) {
		return false;
	}
	Player* player = ctx.players[playerIndex];
	if (!playerHasCard(player, "Hajr")) return false;

	WindowGuard guard(*this, ReactionWindow::AfterMovement);

	bool playHajr = true; // AI default: always play
	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "yn";
		req.actor_index = playerIndex;
		req.prompt = player->getFactionName() +
			", play Hajr for a second movement this phase?";
		auto resp = ctx.adapter->requestDecision(req);
		playHajr = resp && resp->valid && resp->payload_json == "y";
	}
	if (!playHajr) return false;

	player->removeTreacheryCard("Hajr");
	logWindowOpen(ctx, ReactionWindow::AfterMovement,
		player->getFactionName() + " played Hajr");
	if (ctx.logger) {
		ctx.logger->logDebug("[Hajr] " + player->getFactionName() +
			" gains one extra movement action.");
	}
	return true;
}

// --- Battle: AfterBattleResolution (Harkonnen capture) -------------------

void ReactionEngine::dispatchAfterBattleResolution(PhaseContext& ctx,
	int winnerIdx, int loserIdx) {
	if (winnerIdx < 0 || winnerIdx >= static_cast<int>(ctx.players.size())) {
		return;
	}

	WindowGuard guard(*this, ReactionWindow::AfterBattleResolution);

	if (ctx.featureSettings.advancedFactionAbilities) {
		FactionAbility* ability = ctx.players[winnerIdx]->getFactionAbility();
		if (ability) ability->onBattleWon(ctx, loserIdx);
	}
}

// --- Battle: BeforeBattlePlanReveal (Voice + Peek setup) -----------------

void ReactionEngine::dispatchBeforeBattlePlanReveal(PhaseContext& ctx,
	int attackerIdx, int defenderIdx) {
	WindowGuard guard(*this, ReactionWindow::BeforeBattlePlanReveal);
	(void)ctx;
	(void)attackerIdx;
	(void)defenderIdx;
	// State machines for Voice and Peek live in battle_phase.cpp. This is a
	// pure window marker — legality checks see the window, but nothing here
	// mutates state and we don't emit a log line (the existing Voice/Peek
	// log lines already document the in-window decisions).
}

// --- AnytimeSafe checkpoint (Tleilaxu Ghola) -----------------------------

void ReactionEngine::dispatchAnytimeSafe(PhaseContext& ctx,
	const std::string& checkpointLabel) {
	bool anyHolder = false;
	for (Player* p : ctx.players) {
		if (playerHasCard(p, "Tleilaxu Ghola")) { anyHolder = true; break; }
	}
	if (!anyHolder) return;

	WindowGuard guard(*this, ReactionWindow::AnytimeSafe);

	// Full Ghola revival mechanics are deferred. The window is open here
	// (so isReactionLegalNow("Tleilaxu Ghola") returns true), but no prompt
	// is issued yet — interactive Ghola play lands alongside the revival API.
	(void)ctx;
	(void)checkpointLabel;
}
