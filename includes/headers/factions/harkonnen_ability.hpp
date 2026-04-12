#pragma once
#include "factions/faction_ability.hpp"
#include "leader.hpp"
#include <vector>

class HarkonnenAbility : public FactionAbility {
public:
	std::string getFactionName() const override;

	// --- Bidding Phase hooks ---
	int getMaxTreacheryCards() const override;
	int getStartingTreacheryCardCount() const override;
	void onCardWonAtAuction(PhaseContext& ctx) override;

	// --- Traitor hooks ---
	bool keepsAllTraitorCards() const override;

	// --- Battle Phase hooks ---
	// Called after Harkonnen wins a battle to capture a leader
	void onBattleWon(PhaseContext& ctx, int opponentIndex) override;
	
	// --- Initialization ---
	void setupAtStart(Player* player) override;
	void placeStartingForces(PhaseContext& ctx) override;

	// --- Captured Leaders management (explicit getter/setter pattern) ---
	const std::vector<Leader>& getCapturedLeaders() const;
	void addCapturedLeader(const Leader& leader);
	void removeCapturedLeader(int capturedIndex);
	void clearCapturedLeaders();
	bool hasCapturedLeaders() const;

private:
	std::vector<Leader> capturedLeaders;
};


