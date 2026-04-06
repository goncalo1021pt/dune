#pragma once

#include "phase.hpp"
#include "settings.hpp"

class StormPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	void moveStorm(int sectorsToMove, int& stormSector);
	int drawStormCard(std::vector<int>& stormDeck, std::mt19937& rng);
	void initializeStormDeck(std::vector<int>& stormDeck, std::mt19937& rng);
};
