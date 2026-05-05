#pragma once

#include <string>
#include <vector>

#include "phase.hpp"

class SpiceBlowPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

	// Public utilities reused by reaction-driven worm placements (Fremen
	// advanced Karama). Static so callers don't need a SpiceBlowPhase
	// instance.
	static std::vector<std::string> getDesertTerritories(const GameMap& map);
	static void resolveWormOnTerritory(const std::string& territoryName, GameMap& map, PhaseContext* ctxPtr);
};
