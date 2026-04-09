#include "factions/harkonnen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "cards/treachery_deck.hpp"
#include "map.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"
#include <algorithm>

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
		// No leaders to capture, default to 2 spice
		harkonnen->addSpice(2);
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"[Harkonnen] Opponent has no leaders, takes 2 spice",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = "Harkonnen";
			e.spiceValue = 2;
			ctx.logger->logEvent(e);
		}
		return;
	}
	
	// TODO: Interactive choice: capture a leader or take 2 spice
	// For now, store leader index and take 2 spice as default
	// When battle phase implements interactive choice, call:
	//   harkonnen->getFactionAbility()->addCapturedLeader(leaderIndex);
	
	harkonnen->addSpice(2);
	if (ctx.logger) {
		Event e(EventType::BATTLE_RESOLVED,
			"[Harkonnen] Takes 2 spice from battle victory",
			ctx.turnNumber, "BATTLE");
		e.playerFaction = "Harkonnen";
		e.spiceValue = 2;
		ctx.logger->logEvent(e);
	}
}

// --- Captured Leaders management ---
const std::vector<int>& HarkonnenAbility::getCapturedLeaders() const {
	return capturedLeaderIndices;
}

void HarkonnenAbility::addCapturedLeader(int leaderIndex) {
	capturedLeaderIndices.push_back(leaderIndex);
}

void HarkonnenAbility::removeCapturedLeader(int leaderIndex) {
	auto it = std::find(capturedLeaderIndices.begin(), capturedLeaderIndices.end(), leaderIndex);
	if (it != capturedLeaderIndices.end()) {
		capturedLeaderIndices.erase(it);
	}
}

void HarkonnenAbility::clearCapturedLeaders() {
	capturedLeaderIndices.clear();
}

bool HarkonnenAbility::hasCapturedLeaders() const {
	return !capturedLeaderIndices.empty();
}
