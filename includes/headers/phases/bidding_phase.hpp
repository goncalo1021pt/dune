#pragma once

#include "phase.hpp"
#include <vector>
#include <set>

/**
 * BiddingPhase: Players bid for treachery cards using a risk/retreat mechanism
 * 
 * Rules:
 * - For each treachery card (playerCount auctions):
 *   - Draw card from treachery deck
 *   - Players bid incrementally
 *   - Players can raise bid or pass (pass = drop out of ONLY that card)
 *   - Last player standing wins the card and pays the bid in spice
 *   - Each new card starts with a different player (round-robin)
 * 
 * Special cases:
 * - Player with 0 spice can participate but can only pass
 * - Max 4 treachery cards per player (warn if exceeded)
 * - Opening bid always starts at 0
 */
class BiddingPhase : public Phase {
private:
	/**
	 * Run one card's auction from start to finish
	 * Returns the player index of the winner, or -1 if all players passed
	 */
	int biddingRoundForCard(PhaseContext& ctx, int startingPlayerIndex, 
	                         const std::vector<int>& eligiblePlayers);

	/**
	 * Get the next active player in round-robin order
	 * Returns index of next active player, or -1 if no active players
	 */
	int getNextActivePlayer(const std::set<int>& activePlayers, int currentIndex, int playerCount);

	/**
	 * Ask a player to bid or pass (interactive or AI)
	 * Returns true if player raises bid, false if player passes
	 */
	bool askPlayerToBid(PhaseContext& ctx, int playerIndex, int currentBid, int& newBid);

	/**
	 * AI decision logic for bidding
	 * Simple heuristic: random or spice-based, updates newBid if raising
	 */
	bool aiDecideToBid(PhaseContext& ctx, int playerIndex, int currentBid, int& newBid);

public:
	void execute(PhaseContext& ctx) override;
};
