#include "factions/harkonnen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "cards/treachery_deck.hpp"
#include "map.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"
#include "interaction/interaction_adapter.hpp"
#include <algorithm>
#include <iostream>

std::string HarkonnenAbility::getFactionName() const {
	return "Harkonnen";
}

int HarkonnenAbility::getMaxTreacheryCards() const {
	return 8;  // Harkonnen can hold 8 cards (vs 4 for others)
}

int HarkonnenAbility::getStartingTreacheryCardCount() const {
	return 2;  // Harkonnen start with 2 cards (vs 1 for others)
}

bool HarkonnenAbility::keepsAllTraitorCards() const {
	return true;  // Harkonnen keep ALL traitor cards they draw
}

void HarkonnenAbility::onCardWonAtAuction(PhaseContext& ctx) {
	// Find Harkonnen player (the one calling this will be the winning bidder, but we verify)
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return;
	
	Player* harkonnen = ctx.players[harkonnenIndex];
	int currentHandSize = harkonnen->getTreacheryCards().size();
	
	if (currentHandSize < getMaxTreacheryCards()) {
		treacheryCard bonusCard = ctx.treacheryDeck.drawCard();
		harkonnen->addTreacheryCard(bonusCard.name);
		
		if (ctx.logger) {
			Event e(EventType::BID_PLACED,
				"[Harkonnen bonus] " + bonusCard.name + " drawn (now " + std::to_string(harkonnen->getTreacheryCards().size()) + "/8 cards)",
				ctx.turnNumber, "BIDDING");
			e.playerFaction = "Harkonnen";
			ctx.logger->logEvent(e);
		}
	} else if (ctx.logger) {
		Event e(EventType::BID_PLACED,
			"[Harkonnen] Already at max 8 cards, no bonus",
			ctx.turnNumber, "BIDDING");
		e.playerFaction = "Harkonnen";
		ctx.logger->logEvent(e);
	}
}

void HarkonnenAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(10);  // Harkonnen start with 10 spice
}

void HarkonnenAbility::placeStartingForces(PhaseContext& ctx) {
	// Find Harkonnen player index
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return;
	
	Player* harkonnen = ctx.players[harkonnenIndex];
	
	// Place 10 units in Carthag (sector -1 auto-selects first sector)
	ctx.map.addUnitsToTerritory("Carthag", harkonnenIndex, 10, 0, -1);
	harkonnen->deployUnits(10);
	
	// Remaining 10 units in reserve
}

void HarkonnenAbility::onBattleWon(PhaseContext& ctx, int opponentIndex) {
	if (opponentIndex < 0 || opponentIndex >= (int)ctx.players.size()) return;
	
	// Find Harkonnen player
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return;
	
	Player* harkonnen = ctx.players[harkonnenIndex];
	Player* opponent = ctx.players[opponentIndex];
	
	// Get opponent's alive leaders
	const auto& opponentLeaders = opponent->getAliveLeaders();
	if (opponentLeaders.empty()) {
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"[Harkonnen] Opponent has no leaders to capture",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = "Harkonnen";
			ctx.logger->logEvent(e);
		}
		return;
	}

	int captureIdx = 0;
	if (ctx.adapter) {
		std::vector<std::string> options;
		for (const auto& leader : opponentLeaders) {
			options.push_back(leader.name + " (power:" + std::to_string(leader.power) + ")");
		}
		DecisionRequest req;
		req.kind = "select";
		req.actor_index = harkonnenIndex;
		req.prompt = "[Harkonnen] Choose leader to capture from " + opponent->getFactionName() + ":";
		req.options = options;
		auto resp = ctx.adapter->requestDecision(req);
		if (resp && resp->valid) {
			for (int i = 0; i < static_cast<int>(options.size()); ++i) {
				if (options[i] == resp->payload_json) { captureIdx = i; break; }
			}
		}
	} else {
		// AI: capture highest-power leader.
		for (int i = 1; i < static_cast<int>(opponentLeaders.size()); ++i) {
			if (opponentLeaders[i].power > opponentLeaders[captureIdx].power) {
				captureIdx = i;
			}
		}
	}

	std::vector<Leader>& mutableOppLeaders = opponent->getAliveLeadersMutable();
	if (captureIdx < 0 || captureIdx >= static_cast<int>(mutableOppLeaders.size())) {
		return;
	}

	Leader captured = mutableOppLeaders[captureIdx];
	addCapturedLeader(captured);
	mutableOppLeaders.erase(mutableOppLeaders.begin() + captureIdx);

	if (ctx.logger) {
		Event e(EventType::BATTLE_RESOLVED,
			"[Harkonnen] Captures leader " + captured.name + " from " + opponent->getFactionName(),
			ctx.turnNumber, "BATTLE");
		e.playerFaction = "Harkonnen";
		ctx.logger->logEvent(e);
	}

	// Immediately after capture, Harkonnen may execute the captured leader for 2 spice.
	bool executeCapturedLeader = false;
	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "yn";
		req.actor_index = harkonnenIndex;
		req.prompt = "[Harkonnen] Execute captured leader " + captured.name + " for 2 spice?";
		auto resp = ctx.adapter->requestDecision(req);
		executeCapturedLeader = resp && resp->valid && resp->payload_json == "y";
	}

	if (executeCapturedLeader) {
		removeCapturedLeader(static_cast<int>(capturedLeaders.size()) - 1);
		harkonnen->addSpice(2);

		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"[Harkonnen] Executes captured leader " + captured.name + " for 2 spice",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = "Harkonnen";
			e.spiceValue = 2;
			ctx.logger->logEvent(e);
		}
	}
}

// --- Captured Leaders management ---
const std::vector<Leader>& HarkonnenAbility::getCapturedLeaders() const {
	return capturedLeaders;
}

void HarkonnenAbility::addCapturedLeader(const Leader& leader) {
	capturedLeaders.push_back(leader);
}

void HarkonnenAbility::removeCapturedLeader(int capturedIndex) {
	if (capturedIndex >= 0 && capturedIndex < static_cast<int>(capturedLeaders.size())) {
		capturedLeaders.erase(capturedLeaders.begin() + capturedIndex);
	}
}

void HarkonnenAbility::clearCapturedLeaders() {
	capturedLeaders.clear();
}

bool HarkonnenAbility::hasCapturedLeaders() const {
	return !capturedLeaders.empty();
}
