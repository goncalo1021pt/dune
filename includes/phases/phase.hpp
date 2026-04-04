#pragma once

#include "phase_context.hpp"

/**
 * Phase: Abstract base class for all game phases.
 * Each phase (STORM, SPICE_BLOW, etc.) inherits and implements execute().
 * Uses Strategy pattern with Dependency Injection via PhaseContext.
 */
class Phase {
public:
	virtual ~Phase() = default;

	/**
	 * Execute this phase's logic.
	 * @param ctx PhaseContext containing all shared game state refs.
	 */
	virtual void execute(PhaseContext& ctx) = 0;
};
