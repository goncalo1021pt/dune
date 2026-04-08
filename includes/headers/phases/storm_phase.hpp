#pragma once

#include "phase.hpp"
#include "settings.hpp"

class StormPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	void moveStorm(int sectorsToMove, int& stormSector);
	int  drawStormCard(std::vector<int>& stormDeck, std::mt19937& rng);
	void initializeStormDeck(std::vector<int>& stormDeck, std::mt19937& rng);

	// Apply storm damage to all units in swept sectors.
	// prevSector: the sector the storm occupied BEFORE moving this turn.
	// move:       the number of sectors travelled (from the storm card).
	void applyStormDamage(PhaseContext& ctx, int prevSector, int move);
};