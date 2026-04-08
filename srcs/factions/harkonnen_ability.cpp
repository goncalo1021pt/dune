#include "factions/harkonnen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "cards/treachery_deck.hpp"
#include "map.hpp"
#include "events/event.hpp"
#include "logger/event_logger.hpp"
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
	
	// If Harkonnen has all own leaders killed, must return all captured leaders
	if (hasAllOwnLeadersKilled(ctx)) {
		returnAllCapturedLeaders(ctx);
		if (ctx.logger) {
			ctx.logger->logDebug("[Harkonnen] All own leaders killed, must return all captured leaders");
		}
		return;
	}
	
	// Get mutable reference to opponent's alive leaders
	auto& opponentLeaders = opponent->getAliveLeadersMutable();
	if (opponentLeaders.empty()) return;
	
	// Interactive choice: steal a leader or take 2 spice
	if (ctx.interactiveMode) {
		std::cout << "\n[Harkonnen] " << harkonnen->getFactionName() << " won battle against " 
				  << opponent->getFactionName() << "!\n";
		std::cout << "Choose an option:\n";
		std::cout << "  0: No reward\n";
		
		// Display opponent's alive leaders
		for (size_t i = 0; i < opponentLeaders.size(); ++i) {
			std::cout << "  " << (i + 1) << ": Capture " << opponentLeaders[i].name 
			          << " (power:" << opponentLeaders[i].power << ")\n";
		}
		std::cout << "Enter choice (0-" << opponentLeaders.size() << "): ";
		
		int choice;
		std::cin >> choice;
		
		// No reward
		if (choice == 0) {
			if (ctx.logger) {
				Event e(EventType::BATTLE_RESOLVED,
					"[Harkonnen] Declines reward",
					ctx.turnNumber, "BATTLE");
				e.playerFaction = "Harkonnen";
				ctx.logger->logEvent(e);
			}
			std::cout << "  No reward taken\n";
		}
		// Capture leader
		else if (choice >= 1 && choice <= (int)opponentLeaders.size()) {
			int leaderIdx = choice - 1;
			Leader capturedLeader = opponentLeaders[leaderIdx];
			
			// Decision prompt: keep or kill for spice
			std::cout << "\n[Captured Leader] " << capturedLeader.name << " (power:" << capturedLeader.power << ")\n";
			std::cout << "What do you want to do?\n";
			std::cout << "  0: Keep for later use in battle\n";
			std::cout << "  1: Kill for 2 spice\n";
			std::cout << "Enter choice (0-1): ";
			
			int keepOrKill;
			std::cin >> keepOrKill;
			
			if (keepOrKill == 0) {
				// Keep the leader
				addCapturedLeader(capturedLeader);
				opponentLeaders.erase(opponentLeaders.begin() + leaderIdx);
				
				if (ctx.logger) {
					Event e(EventType::BATTLE_RESOLVED,
						"[Harkonnen] Captured and keeps " + capturedLeader.name + " from " + opponent->getFactionName(),
						ctx.turnNumber, "BATTLE");
					e.playerFaction = "Harkonnen";
					ctx.logger->logEvent(e);
				}
				std::cout << "  Kept: " << capturedLeader.name << " (stored for later use)\n";
			} else {
				// Kill for spice
				harkonnen->addSpice(2);
				opponentLeaders.erase(opponentLeaders.begin() + leaderIdx);
				
				if (ctx.logger) {
					Event e(EventType::BATTLE_RESOLVED,
						"[Harkonnen] Killed captured " + capturedLeader.name + " for 2 spice",
						ctx.turnNumber, "BATTLE");
					e.playerFaction = "Harkonnen";
					e.spiceValue = 2;
					ctx.logger->logEvent(e);
				}
				std::cout << "  Killed " << capturedLeader.name << " for 2 spice\n";
			}
		}
	}
	// Non-interactive: default to taking 2 spice
	else {
		harkonnen->addSpice(2);
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"[Harkonnen] Takes 2 spice (non-interactive mode)",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = "Harkonnen";
			e.spiceValue = 2;
			ctx.logger->logEvent(e);
		}
	}
}

std::vector<Leader> HarkonnenAbility::getCapturedLeaders() const {
	return capturedLeaders;
}

void HarkonnenAbility::addCapturedLeader(const Leader& leader) {
	capturedLeaders.push_back(leader);
}

void HarkonnenAbility::returnAllCapturedLeaders(PhaseContext& ctx) {
	(void)ctx;  // Suppress unused parameter warning
	
	if (capturedLeaders.empty()) return;
	
	// TODO: Properly return captured leaders to their original factions
	// For now, they're just cleared from Harkonnen's captured list
	// This will be properly implemented when Tleilaxu tanks system is added
	
	if (ctx.logger) {
		Event e(EventType::LEADER_KILLED,
			"[Harkonnen] Must return " + std::to_string(capturedLeaders.size()) + " captured leaders (all own leaders dead)",
			ctx.turnNumber, "");
		e.playerFaction = "Harkonnen";
		ctx.logger->logEvent(e);
	}
	
	capturedLeaders.clear();
}

bool HarkonnenAbility::hasAllOwnLeadersKilled(PhaseContext& ctx) const {
	// Find Harkonnen player
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return false;
	
	const auto& leaders = ctx.players[harkonnenIndex]->getAliveLeaders();
	return leaders.empty();
}
