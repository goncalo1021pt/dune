#pragma once

#include "phase.hpp"

class ChoamCharityPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;
};

class RevivalPhase : public Phase {
public:
	void execute(PhaseContext& ctx) override;
};
