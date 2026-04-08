#include <phases/bidding_phase.hpp>
#include <phases/phase_context.hpp>
#include <cards/treachery_deck.hpp>
#include <player.hpp>
#include <set>
#include <random>
#include "events/event.hpp"
#include "logger/event_logger.hpp"
#include <iostream>

void BiddingPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("BIDDING Phase");
	}

	// Get view for this phase
	auto view = ctx.getBiddingView();

	// Step 1: Declare hand counts (for transparency)
	if (ctx.logger) {
		ctx.logger->logDebug("=== Declaration ===");
	}
	std::vector<int> eligiblePlayers; // Players who can bid (< 4 cards)
	
	for (size_t i = 0; i < view.players.size(); ++i) {
		int handCount = view.players[i]->getTreacheryCards().size();
		FactionAbility* ability = ctx.getAbility(i);
		int maxCards = (ability) ? ability->getMaxTreacheryCards() : 4;
		
		std::string playerStatus = view.players[i]->getFactionName() + ": " + std::to_string(handCount) + "/" + std::to_string(maxCards) + " cards";
		
		if (handCount >= maxCards) {
			playerStatus += " (MUST PASS)";
		} else {
			eligiblePlayers.push_back(i);
		}
		if (ctx.logger) {
			ctx.logger->logDebug(playerStatus);
		}
	}

	// Step 2: If no eligible players, bidding phase ends
	if (eligiblePlayers.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug("No players eligible to bid (all at 4 card limit)");
		}
		return;
	}

	// Step 3: Dealer deals cards (one for each eligible player)
	if (ctx.logger) {
		ctx.logger->logDebug("=== Dealing Cards ===");
	}
	std::vector<treacheryCard> auctionCards;
	for (size_t i = 0; i < eligiblePlayers.size(); ++i) {
		treacheryCard card = view.treacheryDeck.drawCard();
		auctionCards.push_back(card);
		if (ctx.logger) {
			ctx.logger->logDebug("Card " + std::to_string(i + 1) + " dealt (face down)");
		}
	}

	// Step 4: Auction each card
	if (ctx.logger) {
		ctx.logger->logDebug("=== Auction ===");
	}
	int startingBidderIndex = 0; // Index in eligiblePlayers, will rotate

	for (size_t cardIdx = 0; cardIdx < auctionCards.size(); ++cardIdx) {
		if (ctx.logger) {
			ctx.logger->logDebug("Card " + std::to_string(cardIdx + 1) + ": " + auctionCards[cardIdx].name);
		}

		// Find next eligible player to start bidding
		int startingPlayerIdx = (startingBidderIndex) % eligiblePlayers.size();
		int startingPlayer = eligiblePlayers[startingPlayerIdx];

		// Run bidding for this card
		int winner = biddingRoundForCard(ctx, startingPlayer, eligiblePlayers);

		if (winner == -1) {
			// Everyone passed on this card - return all remaining cards to deck and END bidding
			if (ctx.logger) {
				ctx.logger->logDebug("No one bid on this card. Returning remaining cards to deck.");
				ctx.logger->logDebug("=== Bidding Phase Ended ===");
			}
			return;
		}

		// Award card to winner
		view.players[winner]->addTreacheryCard(auctionCards[cardIdx].name);
		if (ctx.logger) {
			Event e(EventType::BID_PLACED,
				view.players[winner]->getFactionName() + " wins \"" + auctionCards[cardIdx].name + "\"",
				ctx.turnNumber, "BIDDING");
			e.playerFaction = view.players[winner]->getFactionName();
			ctx.logger->logEvent(e);
		}

		// Rotate starting bidder for next card
		for (size_t i = 0; i < eligiblePlayers.size(); ++i) {
			if (eligiblePlayers[i] == winner) {
				startingBidderIndex = (i + 1) % eligiblePlayers.size();
				break;
			}
		}
	}

	if (ctx.logger) {
		ctx.logger->logDebug("=== All cards auctioned ===");
	}
}

int BiddingPhase::biddingRoundForCard(PhaseContext& ctx, int startingPlayerIndex, 
                                       const std::vector<int>& eligiblePlayers) {
	auto view = ctx.getBiddingView();

	std::set<int> activeBidders(eligiblePlayers.begin(), eligiblePlayers.end());

	if (activeBidders.empty()) {
		return -1;
	}

	int currentBid = 0;
	int highestBidder = -1;
	int currentPlayerIndex = startingPlayerIndex;
	bool anyoneRaised = false;
	int playersOfferedThisRound = 0;
	int totalEligible = activeBidders.size();

	// First, give everyone a chance to bid initially
	// Then continue bidding with remaining players until only 1 remains
	while (!activeBidders.empty()) {
		// Get next active player in round-robin
		currentPlayerIndex = getNextActivePlayer(activeBidders, currentPlayerIndex, view.players.size());

		if (currentPlayerIndex < 0) {
			break;
		}

		playersOfferedThisRound++;

		// Player decides: bid or pass
		int newBid = currentBid;
		bool raises = askPlayerToBid(ctx, currentPlayerIndex, currentBid, newBid);

		if (raises) {
			currentBid = newBid;
			highestBidder = currentPlayerIndex;
			anyoneRaised = true;
		} else {
			// Player passes - remove from active bidders
			activeBidders.erase(currentPlayerIndex);
		}

		// If this was the first round and everyone passed without anyone raising, end bidding
		if (playersOfferedThisRound >= totalEligible && !anyoneRaised) {
			if (ctx.logger) {
				ctx.logger->logDebug("Everyone passed on this card.");
			}
			return -1;
		}

		// If only 1 player remains and someone has raised, they win
		if (activeBidders.size() == 1 && anyoneRaised) {
			highestBidder = *activeBidders.begin();
			
			if (ctx.logger) {
				std::string winMsg = "Winner: " + view.players[highestBidder]->getFactionName() + 
					" (bid: " + std::to_string(currentBid) + " spice)";
				if (currentBid > 0) {
					int spaceBefore = view.players[highestBidder]->getSpice();
					view.players[highestBidder]->removeSpice(currentBid);
					int spaceAfter = view.players[highestBidder]->getSpice();
					winMsg += " - pays " + std::to_string(currentBid) + " spice (" + 
						std::to_string(spaceBefore) + " -> " + std::to_string(spaceAfter) + ")";
				}
				ctx.logger->logDebug(winMsg);
			}
			
			return highestBidder;
		}
	}

	return -1;
}

int BiddingPhase::getNextActivePlayer(const std::set<int>& activePlayers, int currentIndex, int) {
	// Find next active player after currentIndex in round-robin
	auto it = activePlayers.upper_bound(currentIndex);

	if (it != activePlayers.end()) {
		return *it;
	}

	// Wrap around to start
	if (!activePlayers.empty()) {
		return *activePlayers.begin();
	}

	return -1;
}

bool BiddingPhase::askPlayerToBid(PhaseContext& ctx, int playerIndex, int currentBid, int& newBid) {
	auto view = ctx.getBiddingView();
	Player* player = view.players[playerIndex];

	if (view.interactiveMode) {
		// Interactive mode: prompt player for input
		std::string prompt = player->getFactionName() + "'s turn to bid (current bid: " + 
			std::to_string(currentBid) + " spice). Your spice: " + std::to_string(player->getSpice());
		if (ctx.logger) {
			ctx.logger->logDebug(prompt);
			ctx.logger->logDebug("Enter bid amount (0 to pass): ");
		}
		
		int bid = -1;
		while (bid < 0) {
			std::cin >> bid;
			
			if (std::cin.fail()) {
				std::cin.clear();
				std::cin.ignore(10000, '\n');
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Enter a number or 0 to pass: ");
				}
				bid = -1;
				continue;
			}
			
			if (bid == 0) {
				if (ctx.logger) {
					ctx.logger->logDebug(player->getFactionName() + " passes.");
				}
				return false; // Pass
			}
			
			if (bid <= currentBid) {
				if (ctx.logger) {
					ctx.logger->logDebug("Bid must be higher than current bid (" + std::to_string(currentBid) + "). Try again: ");
				}
				bid = -1;
				continue;
			}
			
			if (bid > player->getSpice()) {
				if (ctx.logger) {
					ctx.logger->logDebug("Bid (" + std::to_string(bid) + ") exceeds your spice (" + 
						std::to_string(player->getSpice()) + "). Try again: ");
				}
				bid = -1;
				continue;
			}
		}
		
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " bids " + std::to_string(bid) + " spice.");
		}
		newBid = bid;
		return true; // Raise
	} else {
		// AI decision
		return aiDecideToBid(ctx, playerIndex, currentBid, newBid);
	}
}

bool BiddingPhase::aiDecideToBid(PhaseContext& ctx, int playerIndex, int currentBid, int& newBid) {
	auto view = ctx.getBiddingView();
	Player* player = view.players[playerIndex];

	// If player doesn't have spice to bid higher, must pass
	if (player->getSpice() <= currentBid) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " passes (insufficient spice)");
		}
		return false;
	}

	// Simple AI: 40% pass rate for basic strategy
	std::uniform_int_distribution<> dist(1, 100);
	int roll = dist(view.rng);

	if (roll <= 40) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " passes");
		}
		return false; // Pass
	}

	// AI raises by 1-5 spice randomly
	std::uniform_int_distribution<> bidIncrementDist(1, 5);
	int bidIncrement = bidIncrementDist(view.rng);
	int bid = currentBid + bidIncrement;
	
	// Cap at available spice
	if (bid > player->getSpice()) {
		bid = player->getSpice();
	}
	
	if (ctx.logger) {
		ctx.logger->logDebug(player->getFactionName() + " bids " + std::to_string(bid) + " spice");
	}
	newBid = bid;
	return true; // Raise
}
