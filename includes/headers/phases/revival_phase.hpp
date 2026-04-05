#pragma once

#include "phase.hpp"

/**
 * RevivalPhase: Destroyed units are revived
 * 
 * Rules:
 * - Each faction can revive up to 3 units per turn
 * - First revives are free (depends on faction modifier)
 * - Additional revives cost 2 spice each
 */
class RevivalPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;
};
