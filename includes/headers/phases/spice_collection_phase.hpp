#pragma once

#include "phase.hpp"

/**
 * SpiceCollectionPhase: Players collect spice from territories
 * 
 * Process:
 * 1. Three cities generate spice each turn:
 *    - Arrakeen: +2 spice
 *    - Carthag: +2 spice
 *    - Tuek's Sietch: +1 spice
 * 
 * 2. Each faction's units collect spice:
 *    - Each unit collects 2 spice from its territory
 *    - Spice is removed from territory and given to player
 */
class SpiceCollectionPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	/**
	 * Add spice production from the three special cities
	 */
	void addCitySpiceProduction(PhaseContext& ctx);

	/**
	 * Collect spice from all territories with units
	 */
	void collectSpiceFromTerritories(PhaseContext& ctx);
};
