#pragma once

#include "phase.hpp"

class SpiceBlowPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;

private:
	void resolveWormOnTerritory(const std::string& territoryName, GameMap& map, PhaseContext* ctxPtr);
};
