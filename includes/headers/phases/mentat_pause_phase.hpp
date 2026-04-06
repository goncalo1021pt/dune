#pragma once

#include "phase.hpp"

/**
 * MentatPausePhase: End-of-turn reflection and victory check
 * 
 * Responsibilities:
 * - Check victory conditions (3+ cities controlled)
 * - Allow players to review game state
 * - Prepare for next turn
 * 
 * Victory Condition (Phase 1):
 * - Any player who controls 3 or more cities wins the game
 * 
 * Future Enhancements:
 * - Alliance victory conditions
 * - Faction-specific win conditions
 * - Prestige/influence scoring
 */
class MentatPausePhase : public Phase {
private:
	bool checkVictory(PhaseContext& ctx);
	int countCitiesControlled(PhaseContext& ctx, int factionIndex);

public:
	void execute(PhaseContext& ctx) override;
};
