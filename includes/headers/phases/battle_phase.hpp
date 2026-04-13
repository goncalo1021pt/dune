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
		struct BattleOutcomeInputs {
			int attackerIdx = -1;
			int defenderIdx = -1;
			std::string territoryName;
			int winnerIdx = -1;
			int loserIdx = -1;
			bool winnerIsAttacker = true;
			bool isTie = false;
			int attackerUnits = 0;
			int defenderUnits = 0;
			int normalUnits = 0;
			int eliteUnits = 0;
			int defNormalUnits = 0;
			int defEliteUnits = 0;
			int unitStrength = 0;
			int defUnitStrength = 0;
			int attackerEliteStrength = 1;
			int defenderEliteStrength = 1;
		};

		// Main battle resolution for a contested territory
		void resolveBattle(PhaseContext& ctx, int attackerIdx, const std::string& territoryName);
		void applyBattleOutcome(PhaseContext& ctx, const BattleOutcomeInputs& input);
		
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

};
