#pragma once

#include "phase.hpp"
#include <map.hpp>
#include <leader.hpp>

class BattlePhase : public Phase {
	public:
		BattlePhase();
		~BattlePhase() override = default;

		void execute(PhaseContext& ctx) override;

	private:
		// Main battle resolution for a contested territory
		void resolveBattle(PhaseContext& ctx, int attackerIdx, const std::string& territoryName);
		
		// Get player's battle wheel choice (0 to max units)
		int getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits,
		                         const std::string& territoryName = "", int eliteStrength = 2);
		
		// Calculate actual unit strength (elite units count as 2x)
		int calculateUnitStrength(int commitUnits, int normalAvailable, int eliteAvailable, int eliteStrength = 2);
		
		// Convert strength value to unit count (inverse of calculateUnitStrength)
		int convertStrengthToUnitCount(int desiredStrength, int normalAvailable, int eliteAvailable, int eliteStrength = 2);
		
		// Ask player how many elite units survive based on remaining strength
		// Returns casualties as (normalKilled, eliteKilled)
		std::pair<int, int> askCasualtyDistribution(PhaseContext& ctx, int playerIndex, int remainingStrength,
		                                             int normalAvailable, int eliteAvailable, int eliteStrength = 2);
		
		// Select leader for battle (must have alive leader) - returns index into alive leaders
		int selectLeaderForBattle(PhaseContext& ctx, int playerIndex, int aliveLeaderCount);
		
};
