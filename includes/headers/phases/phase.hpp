#pragma once

#include "phase_context.hpp"

class Phase {
public:
	virtual ~Phase() = default;
	virtual void execute(PhaseContext& ctx) = 0;
};
