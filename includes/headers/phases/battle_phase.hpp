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
		void resolveBattle(PhaseContext& ctx, int attackerIdx, territory& battleTerritory);
		
		// Battle resolution for territory with N participants
		void resolveBattlesInTerritory(PhaseContext& ctx, territory& territory);
		
		// Helper to get players with units in a territory
		std::vector<int> getPlayersInTerritory(PhaseContext& ctx, const territory& territory);
		
		// Get player's battle wheel choice (0 to max units)
		int getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits);
		
		// Select leader for battle (must have alive leader) - returns index into alive leaders
		int selectLeaderForBattle(PhaseContext& ctx, int playerIndex, int aliveLeaderCount);
		
		// Calculate total battle value (wheel + leader power + card effect)
		int calculateBattleValue(int wheelValue, Leader* leader, int cardEffect);
		
		// Apply casualties to player
		void applyCasualties(PhaseContext& ctx, int playerIndex, const std::string& territoryName, 
		                     int casualtyCount, bool lost);
};
