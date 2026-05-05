#include "reactions/reaction_engine.hpp"
#include "phases/phase_context.hpp"
#include "player.hpp"
#include "factions/faction_ability.hpp"
#include "interaction/interaction_adapter.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"
#include "cards/treachery_deck.hpp"
#include "phases/spice_blow_phase.hpp"
#include "map.hpp"

#include <algorithm>
#include <random>
#include <unordered_set>

namespace {

bool playerHasCard(const Player* p, const std::string& name) {
	const auto& cards = p->getTreacheryCards();
	return std::find(cards.begin(), cards.end(), name) != cards.end();
}

// Static mapping from reaction-card name to the window in which it is legal.
// Cards not listed are treated as not-a-reaction (always illegal via the
// engine; ordinary play during BIDDING/BATTLE is unaffected since those
// plays don't go through ReactionEngine::isReactionLegalNow).
//
// Karama is special-cased: it has multiple legal windows (basic block-an-
// advantage at BeforeFactionAdvantage; advanced per-faction powers at
// AnytimeSafe / BeforeShipment / BeforeBattlePlanReveal). isReactionLegalNow
// handles Karama explicitly rather than going through this helper.
ReactionWindow legalWindowForCard(const std::string& cardName) {
	if (cardName == "Weather Control") return ReactionWindow::BeforeStormMove;
	if (cardName == "Hajr")            return ReactionWindow::AfterMovement;
	if (cardName == "Tleilaxu Ghola")  return ReactionWindow::AnytimeSafe;
	return ReactionWindow::None;
}

// Karama can be played in several windows depending on use:
//   - BeforeFactionAdvantage:  basic "block an advantage" (PR #27)
//   - AnytimeSafe:             advanced powers (Emperor revive, Fremen
//                              sandworm placement, Harkonnen hand-swap)
//   - BeforeShipment:          advanced Guild stop-shipment
//   - BeforeBattlePlanReveal:  advanced Atreides peek-everything
bool isKaramaLegalInWindow(ReactionWindow w) {
	switch (w) {
		case ReactionWindow::BeforeFactionAdvantage:
		case ReactionWindow::AnytimeSafe:
		case ReactionWindow::BeforeShipment:
		case ReactionWindow::BeforeBattlePlanReveal:
			return true;
		default:
			return false;
	}
}

// Worthless treachery card names. Player only stores treachery card names,
// not full treacheryCard structs, so we keep a name-based set for the BG
// canUseWorthlessAsKarama path. Must stay in sync with treachery_deck.cpp's
// WORTHLESS entries.
const std::unordered_set<std::string>& worthlessCardNames() {
	static const std::unordered_set<std::string> names = {
		"Baliset",
		"Jubba Cloak",
		"Kulon",
		"La La La",
		"Trip to Gamont",
	};
	return names;
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
		case ReactionWindow::BeforeFactionAdvantage: return "BeforeFactionAdvantage";
		case ReactionWindow::AnytimeSafe:            return "AnytimeSafe";
		case ReactionWindow::None:                   return "None";
	}
	return "Unknown";
}

bool ReactionEngine::isReactionLegalNow(const std::string& cardName) const {
	if (cardName == "Karama") {
		return isKaramaLegalInWindow(currentWindow_);
	}

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

bool ReactionEngine::applyGholaLeaderRevive(Player& player, std::size_t deadIndex) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Tleilaxu Ghola") == cards.end()) {
		return false;
	}
	if (deadIndex >= player.getDeadLeaders().size()) return false;
	player.reviveLeader(deadIndex);
	player.removeTreacheryCard("Tleilaxu Ghola");
	return true;
}

int ReactionEngine::applyGholaForceRevive(Player& player, int requested) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Tleilaxu Ghola") == cards.end()) {
		return 0;
	}
	int totalDestroyed = player.getUnitsDestroyed() + player.getEliteUnitsDestroyed();
	int n = std::min(requested, 5);
	n = std::min(n, totalDestroyed);
	if (n <= 0) return 0;
	// Player::reviveUnits already revives normals first, then elites.
	player.reviveUnits(n);
	player.removeTreacheryCard("Tleilaxu Ghola");
	return n;
}

namespace {

// Choose force count for a free-revival via adapter int prompt; returns 0
// if the player declines or the adapter is unavailable. hardCap caps the
// upper bound (Ghola = 5, Emperor advanced Karama = 3).
int promptForceReviveCount(PhaseContext& ctx, int playerIndex, int maxAvailable, int hardCap) {
	if (!ctx.adapter) return 0;
	int upper = std::min(hardCap, maxAvailable);
	if (upper <= 0) return 0;
	DecisionRequest req;
	req.kind = "int";
	req.actor_index = playerIndex;
	req.prompt = "How many forces to revive (1-" + std::to_string(upper) + ")? ";
	req.int_min = 1;
	req.int_max = upper;
	auto resp = ctx.adapter->requestDecision(req);
	if (!resp || !resp->valid) return 0;
	try { return std::stoi(resp->payload_json); } catch (...) { return 0; }
}

// Choose dead leader index via adapter select prompt; returns -1 on cancel.
int promptLeaderReviveIndex(PhaseContext& ctx, int playerIndex, const Player& player) {
	if (!ctx.adapter) return -1;
	const auto& dead = player.getDeadLeaders();
	if (dead.empty()) return -1;
	DecisionRequest req;
	req.kind = "select";
	req.actor_index = playerIndex;
	req.prompt = "Choose a leader to revive:";
	for (const auto& l : dead) req.options.push_back(l.name);
	auto resp = ctx.adapter->requestDecision(req);
	if (!resp || !resp->valid || resp->payload_json.empty()) return -1;
	for (std::size_t i = 0; i < dead.size(); ++i) {
		if (dead[i].name == resp->payload_json) return static_cast<int>(i);
	}
	return -1;
}

} // namespace

void ReactionEngine::dispatchAnytimeSafe(PhaseContext& ctx,
	const std::string& checkpointLabel) {
	// Returns true if at least one player holds a card whose effect can
	// fire here: Tleilaxu Ghola (any faction), or a real Karama in the
	// hand of a faction with an AnytimeSafe advanced power (Emperor /
	// Fremen / Harkonnen — BG has no advanced Karama power).
	auto anyPlayAvailable = [](const std::vector<Player*>& players) {
		for (Player* p : players) {
			if (playerHasCard(p, "Tleilaxu Ghola")) return true;
			if (playerHasCard(p, "Karama")) {
				auto* ability = p->getFactionAbility();
				if (!ability) continue;
				const std::string& name = ability->getFactionName();
				if (name == "Emperor" || name == "Fremen" || name == "Harkonnen") {
					return true;
				}
			}
		}
		return false;
	};
	if (!anyPlayAvailable(ctx.players)) return;

	WindowGuard guard(*this, ReactionWindow::AnytimeSafe);
	(void)checkpointLabel;

	// AI default (no adapter): never play Ghola or advanced Karama —
	// preserves seed-42 regression and matches the conservative "do
	// nothing" policy used elsewhere.
	if (!ctx.adapter) return;

	for (int idx : ctx.turnOrder) {
		Player* player = ctx.players[idx];
		if (!playerHasCard(player, "Tleilaxu Ghola")) continue;

		bool hasDeadLeader = !player->getDeadLeaders().empty();
		int destroyedForces = player->getUnitsDestroyed() + player->getEliteUnitsDestroyed();
		if (!hasDeadLeader && destroyedForces <= 0) continue;

		// Ask whether to play.
		DecisionRequest ynReq;
		ynReq.kind = "yn";
		ynReq.actor_index = idx;
		ynReq.prompt = player->getFactionName() +
			", play Tleilaxu Ghola for an extra revival?";
		auto ynResp = ctx.adapter->requestDecision(ynReq);
		bool play = ynResp && ynResp->valid && ynResp->payload_json == "y";
		if (!play) continue;

		// Choose target type. Only offer "leader" if there is a dead leader,
		// only offer "forces" if there are destroyed units. Both can be true.
		std::string choice;
		if (hasDeadLeader && destroyedForces > 0) {
			DecisionRequest selReq;
			selReq.kind = "select";
			selReq.actor_index = idx;
			selReq.prompt = "Revive a leader or forces?";
			selReq.options = {"leader", "forces"};
			auto selResp = ctx.adapter->requestDecision(selReq);
			if (!selResp || !selResp->valid) continue;
			choice = selResp->payload_json;
		} else if (hasDeadLeader) {
			choice = "leader";
		} else {
			choice = "forces";
		}

		bool applied = false;
		std::string detail;
		if (choice == "leader") {
			int deadIdx = promptLeaderReviveIndex(ctx, idx, *player);
			if (deadIdx < 0) continue;
			std::string leaderName = player->getDeadLeaders()[deadIdx].name;
			applied = applyGholaLeaderRevive(*player, static_cast<std::size_t>(deadIdx));
			if (applied) detail = "leader " + leaderName;
		} else if (choice == "forces") {
			int requested = promptForceReviveCount(ctx, idx, destroyedForces, 5);
			if (requested <= 0) continue;
			int revived = applyGholaForceRevive(*player, requested);
			applied = revived > 0;
			if (applied) detail = std::to_string(revived) + " forces";
		}

		if (!applied) continue;

		logWindowOpen(ctx, ReactionWindow::AnytimeSafe,
			player->getFactionName() + " played Tleilaxu Ghola (" + detail + ")");
		if (ctx.logger) {
			ctx.logger->logDebug("[Ghola] " + player->getFactionName() +
				" revives " + detail);
		}
	}

	// --- Emperor advanced Karama: free 1-leader OR 1-3 force revival ---
	for (int idx : ctx.turnOrder) {
		Player* player = ctx.players[idx];
		auto* ability = player->getFactionAbility();
		if (!ability || ability->getFactionName() != "Emperor") continue;
		// Faction-specific advanced Karama powers require a real Karama in
		// hand; BG worthless-as-Karama only triggers the basic block path.
		if (!playerHasCard(player, "Karama")) continue;

		bool hasDeadLeader = !player->getDeadLeaders().empty();
		int destroyedForces = player->getUnitsDestroyed() + player->getEliteUnitsDestroyed();
		if (!hasDeadLeader && destroyedForces <= 0) continue;

		DecisionRequest ynReq;
		ynReq.kind = "yn";
		ynReq.actor_index = idx;
		ynReq.prompt = player->getFactionName() +
			", play Karama for the Imperial revival (1 leader OR up to 3 forces)?";
		auto ynResp = ctx.adapter->requestDecision(ynReq);
		if (!ynResp || !ynResp->valid || ynResp->payload_json != "y") continue;

		std::string choice;
		if (hasDeadLeader && destroyedForces > 0) {
			DecisionRequest selReq;
			selReq.kind = "select";
			selReq.actor_index = idx;
			selReq.prompt = "Revive a leader or forces?";
			selReq.options = {"leader", "forces"};
			auto selResp = ctx.adapter->requestDecision(selReq);
			if (!selResp || !selResp->valid) continue;
			choice = selResp->payload_json;
		} else if (hasDeadLeader) {
			choice = "leader";
		} else {
			choice = "forces";
		}

		bool applied = false;
		std::string detail;
		if (choice == "leader") {
			int deadIdx = promptLeaderReviveIndex(ctx, idx, *player);
			if (deadIdx < 0) continue;
			std::string leaderName = player->getDeadLeaders()[deadIdx].name;
			applied = applyEmperorKaramaLeaderRevive(*player, ctx.treacheryDeck,
				static_cast<std::size_t>(deadIdx));
			if (applied) detail = "leader " + leaderName;
		} else {
			int requested = promptForceReviveCount(ctx, idx, destroyedForces, 3);
			if (requested <= 0) continue;
			int revived = applyEmperorKaramaForceRevive(*player, ctx.treacheryDeck, requested);
			applied = revived > 0;
			if (applied) detail = std::to_string(revived) + " forces";
		}

		if (!applied) continue;

		logWindowOpen(ctx, ReactionWindow::AnytimeSafe,
			player->getFactionName() + " played Karama for Imperial revival (" + detail + ")");
		if (ctx.logger) {
			ctx.logger->logDebug("[Karama:Imperial Revival] " + player->getFactionName() +
				" revives " + detail);
		}
		break;  // Only one Emperor in a game; no need to keep iterating.
	}

	// --- Fremen advanced Karama: place a sandworm in any desert territory ---
	for (int idx : ctx.turnOrder) {
		Player* player = ctx.players[idx];
		auto* ability = player->getFactionAbility();
		if (!ability || ability->getFactionName() != "Fremen") continue;
		if (!playerHasCard(player, "Karama")) continue;

		std::vector<std::string> deserts = SpiceBlowPhase::getDesertTerritories(ctx.map);
		if (deserts.empty()) continue;

		DecisionRequest ynReq;
		ynReq.kind = "yn";
		ynReq.actor_index = idx;
		ynReq.prompt = player->getFactionName() +
			", play Karama to summon a sandworm to a desert territory?";
		auto ynResp = ctx.adapter->requestDecision(ynReq);
		if (!ynResp || !ynResp->valid || ynResp->payload_json != "y") continue;

		DecisionRequest selReq;
		selReq.kind = "select";
		selReq.actor_index = idx;
		selReq.prompt = "Choose a desert territory to send the sandworm to:";
		selReq.options = deserts;
		auto selResp = ctx.adapter->requestDecision(selReq);
		if (!selResp || !selResp->valid || selResp->payload_json.empty()) continue;
		const std::string& target = selResp->payload_json;
		if (std::find(deserts.begin(), deserts.end(), target) == deserts.end()) continue;

		if (!applyFremenKaramaSandworm(ctx, *player, ctx.treacheryDeck, target)) continue;

		logWindowOpen(ctx, ReactionWindow::AnytimeSafe,
			player->getFactionName() + " played Karama to summon a worm at " + target);
		if (ctx.logger) {
			ctx.logger->logDebug("[Karama:Sandworm] " + player->getFactionName() +
				" sends a worm to " + target);
		}
		break;  // Only one Fremen in a game.
	}
}

// --- BeforeFactionAdvantage (Karama-block) -------------------------------

std::string ReactionEngine::findKaramaCard(const Player& player) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Karama") != cards.end()) {
		return "Karama";
	}
	auto* ability = player.getFactionAbility();
	if (ability && ability->canUseWorthlessAsKarama()) {
		const auto& worthless = worthlessCardNames();
		for (const auto& c : cards) {
			if (worthless.count(c)) return c;
		}
	}
	return "";
}

bool ReactionEngine::applyKaramaPlay(Player& player, TreacheryDeck& deck,
	const std::string& cardName) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), cardName) == cards.end()) {
		return false;
	}
	player.removeTreacheryCard(cardName);
	deck.discard(cardName);
	return true;
}

bool ReactionEngine::applyEmperorKaramaLeaderRevive(Player& player,
	TreacheryDeck& deck, std::size_t deadIndex) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Karama") == cards.end()) {
		return false;
	}
	if (deadIndex >= player.getDeadLeaders().size()) return false;
	player.reviveLeader(deadIndex);
	player.removeTreacheryCard("Karama");
	deck.discard("Karama");
	return true;
}

int ReactionEngine::applyEmperorKaramaForceRevive(Player& player,
	TreacheryDeck& deck, int requested) {
	const auto& cards = player.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Karama") == cards.end()) {
		return 0;
	}
	int totalDestroyed = player.getUnitsDestroyed() + player.getEliteUnitsDestroyed();
	int n = std::min(requested, 3);  // Imperial revival caps at 3 forces
	n = std::min(n, totalDestroyed);
	if (n <= 0) return 0;
	player.reviveUnits(n);  // normals first, then elites
	player.removeTreacheryCard("Karama");
	deck.discard("Karama");
	return n;
}

bool ReactionEngine::applyFremenKaramaSandworm(PhaseContext& ctx, Player& fremen,
	TreacheryDeck& deck, const std::string& territoryName) {
	const auto& cards = fremen.getTreacheryCards();
	if (std::find(cards.begin(), cards.end(), "Karama") == cards.end()) {
		return false;
	}
	const territory* terr = ctx.map.getTerritory(territoryName);
	if (!terr || terr->terrain != terrainType::desert) return false;

	// Discard the card first so the worm-resolution log lines that follow
	// trail the Karama play in the event stream.
	fremen.removeTreacheryCard("Karama");
	deck.discard("Karama");

	SpiceBlowPhase::resolveWormOnTerritory(territoryName, ctx.map, &ctx);
	return true;
}

bool ReactionEngine::dispatchKaramaBlock(PhaseContext& ctx, int ownerIdx,
	const std::string& advantageLabel) {
	if (ownerIdx < 0 || ownerIdx >= static_cast<int>(ctx.players.size())) {
		return false;
	}

	WindowGuard guard(*this, ReactionWindow::BeforeFactionAdvantage);

	// AI default: never spend a Karama. Preserves seed-42 regression and
	// matches the conservative policy locked in elsewhere.
	if (!ctx.adapter) return false;

	for (int idx : ctx.turnOrder) {
		if (idx == ownerIdx) continue;
		Player* opp = ctx.players[idx];
		std::string karamaCard = findKaramaCard(*opp);
		if (karamaCard.empty()) continue;

		DecisionRequest req;
		req.kind = "yn";
		req.actor_index = idx;
		req.prompt = opp->getFactionName() + ", play " + karamaCard +
			" as Karama to block " + ctx.players[ownerIdx]->getFactionName() +
			"'s " + advantageLabel + "?";
		auto resp = ctx.adapter->requestDecision(req);
		bool play = resp && resp->valid && resp->payload_json == "y";
		if (!play) continue;

		if (!applyKaramaPlay(*opp, ctx.treacheryDeck, karamaCard)) continue;

		logWindowOpen(ctx, ReactionWindow::BeforeFactionAdvantage,
			opp->getFactionName() + " played " + karamaCard +
			" as Karama vs " + ctx.players[ownerIdx]->getFactionName() +
			"'s " + advantageLabel);
		if (ctx.logger) {
			ctx.logger->logDebug("[Karama] " + opp->getFactionName() +
				" blocks " + ctx.players[ownerIdx]->getFactionName() +
				"'s " + advantageLabel + " (discard " + karamaCard + ")");
		}
		return true;
	}
	return false;
}
