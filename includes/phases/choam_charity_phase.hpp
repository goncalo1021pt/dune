#pragma once

#include "phase.hpp"

/**
 * ChoamCharityPhase: CHOAM provides spice to needy factions
 * 
 * Rules:
 * - Any faction with 1 spice or less receives charity to reach 2 spice
 * - Bene Gesserit always receives charity if Bene Gesserit charity is active
 */
class ChoamCharityPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;
};
