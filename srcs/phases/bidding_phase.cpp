#include <phases/bidding_phase.hpp>
#include <phases/phase_context.hpp>
#include <cards/treachery_deck.hpp>
#include <player.hpp>
#include <iostream>
#include <set>
#include <random>

void BiddingPhase::execute(PhaseContext& ctx) {
	std::cout << "  BIDDING Phase" << std::endl;

	// Step 1: Declare hand counts (for transparency)
	std::cout << "\n    === Declaration ===" << std::endl;
	std::vector<int> eligiblePlayers; // Players who can bid (< 4 cards)
	
	for (int i = 0; i < ctx.playerCount; ++i) {
		int handCount = ctx.players[i]->getTreacheryCards().size();
		std::cout << "    " << ctx.players[i]->getFactionName() << ": " << handCount << " cards";
		
		if (handCount >= 4) {
			std::cout << " (MUST PASS)" << std::endl;
		} else {
			std::cout << std::endl;
			eligiblePlayers.push_back(i);
		}
	}

	// Step 2: If no eligible players, bidding phase ends
	if (eligiblePlayers.empty()) {
		std::cout << "\n    No players eligible to bid (all at 4 card limit)" << std::endl;
		return;
	}

	// Step 3: Dealer deals cards (one for each eligible player)
	std::cout << "\n    === Dealing Cards ===" << std::endl;
	std::vector<treacheryCard> auctionCards;
	for (size_t i = 0; i < eligiblePlayers.size(); ++i) {
		treacheryCard card = ctx.treacheryDeck.drawCard();
		auctionCards.push_back(card);
		std::cout << "    Card " << (i + 1) << " dealt (face down)" << std::endl;
	}

	// Step 4: Auction each card
	std::cout << "\n    === Auction ===" << std::endl;
	int startingBidderIndex = 0; // Index in eligiblePlayers, will rotate

	for (size_t cardIdx = 0; cardIdx < auctionCards.size(); ++cardIdx) {
		std::cout << "\n    Card " << (cardIdx + 1) << ": " << auctionCards[cardIdx].name << std::endl;

		// Find next eligible player to start bidding
		int startingPlayerIdx = (startingBidderIndex) % eligiblePlayers.size();
		int startingPlayer = eligiblePlayers[startingPlayerIdx];

		// Run bidding for this card
		int winner = biddingRoundForCard(ctx, startingPlayer, eligiblePlayers);

		if (winner == -1) {
			// Everyone passed on this card - return all remaining cards to deck and END bidding
			std::cout << "\n    No one bid on this card. Returning remaining cards to deck." << std::endl;
			std::cout << "    === Bidding Phase Ended ===" << std::endl;
			return;
		}

		// Award card to winner
		ctx.players[winner]->addTreacheryCard(auctionCards[cardIdx].name);
		std::cout << "    " << ctx.players[winner]->getFactionName() << " wins \"" 
		          << auctionCards[cardIdx].name << "\"" << std::endl;

		// Rotate starting bidder for next card
		for (size_t i = 0; i < eligiblePlayers.size(); ++i) {
			if (eligiblePlayers[i] == winner) {
				startingBidderIndex = (i + 1) % eligiblePlayers.size();
				break;
			}
		}
	}

	std::cout << "\n    === All cards auctioned ===" << std::endl;
}

int BiddingPhase::biddingRoundForCard(PhaseContext& ctx, int startingPlayerIndex, 
                                       const std::vector<int>& eligiblePlayers) {
	std::set<int> activeBidders(eligiblePlayers.begin(), eligiblePlayers.end());

	if (activeBidders.empty()) {
		return -1;
	}

	int currentBid = 0;
	int highestBidder = -1;
	int currentPlayerIndex = startingPlayerIndex;
	bool anyoneRaised = false; // Track if anyone actually bid

	// Bidding loop: continue until only 1 player remains or everyone passes initially
	while (activeBidders.size() > 1) {
		// Get next active player in round-robin
		currentPlayerIndex = getNextActivePlayer(activeBidders, currentPlayerIndex, ctx.playerCount);

		if (currentPlayerIndex < 0) {
			break;
		}

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
	}

	// If no one raised (everyone passed), return -1 to signal all-pass
	if (!anyoneRaised) {
		std::cout << "      Everyone passed on this card." << std::endl;
		return -1;
	}

	// Last player standing is the winner
	if (activeBidders.size() == 1 && anyoneRaised) {
		highestBidder = *activeBidders.begin();
		std::cout << "      Winner: " << ctx.players[highestBidder]->getFactionName() 
		          << " (bid: " << currentBid << " spice)";
		
		if (currentBid > 0) {
			int spaceBefore = ctx.players[highestBidder]->getSpice();
			ctx.players[highestBidder]->removeSpice(currentBid); // Use removeSpice for deduction
			int spaceAfter = ctx.players[highestBidder]->getSpice();
			std::cout << " - pays " << currentBid << " spice (" << spaceBefore << " -> " << spaceAfter << ")";
		}
		std::cout << std::endl;
		
		return highestBidder;
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
	Player* player = ctx.players[playerIndex];

	if (ctx.interactiveMode) {
		// Interactive mode: prompt player for input
		std::cout << "      " << player->getFactionName() << "'s turn to bid (current bid: " << currentBid << " spice)" << std::endl;
		std::cout << "      Your spice: " << player->getSpice() << std::endl;
		std::cout << "      Enter bid amount (0 to pass): ";
		
		int bid = -1;
		while (bid < 0) {
			std::cin >> bid;
			
			if (std::cin.fail()) {
				std::cin.clear();
				std::cin.ignore(10000, '\n');
				std::cout << "      Invalid input. Enter a number or 0 to pass: ";
				bid = -1;
				continue;
			}
			
			if (bid == 0) {
				std::cout << "      " << player->getFactionName() << " passes." << std::endl;
				return false; // Pass
			}
			
			if (bid <= currentBid) {
				std::cout << "      Bid must be higher than current bid (" << currentBid << "). Try again: ";
				bid = -1;
				continue;
			}
			
			if (bid > player->getSpice()) {
				std::cout << "      Bid (" << bid << ") exceeds your spice (" << player->getSpice() << "). Try again: ";
				bid = -1;
				continue;
			}
		}
		
		std::cout << "      " << player->getFactionName() << " bids " << bid << " spice." << std::endl;
		newBid = bid;
		return true; // Raise
	} else {
		// AI decision
		return aiDecideToBid(ctx, playerIndex, currentBid, newBid);
	}
}

bool BiddingPhase::aiDecideToBid(PhaseContext& ctx, int playerIndex, int currentBid, int& newBid) {
	Player* player = ctx.players[playerIndex];

	// If player doesn't have spice to bid higher, must pass
	if (player->getSpice() <= currentBid) {
		std::cout << "      " << player->getFactionName() << " passes (insufficient spice)" << std::endl;
		return false;
	}

	// Simple AI: 40% pass rate for basic strategy
	std::uniform_int_distribution<> dist(1, 100);
	int roll = dist(ctx.rng);

	if (roll <= 40) {
		std::cout << "      " << player->getFactionName() << " passes" << std::endl;
		return false; // Pass
	}

	// AI raises by 1-5 spice randomly
	std::uniform_int_distribution<> bidIncrementDist(1, 5);
	int bidIncrement = bidIncrementDist(ctx.rng);
	int bid = currentBid + bidIncrement;
	
	// Cap at available spice
	if (bid > player->getSpice()) {
		bid = player->getSpice();
	}
	
	std::cout << "      " << player->getFactionName() << " bids " << bid << " spice" << std::endl;
	newBid = bid;
	return true; // Raise
}
