#pragma once

#include <string>

// ReactionWindow names the discrete moments in a turn at which a faction may
// interrupt the normal phase flow with a reaction (treachery card or ability).
//
// Window legality matters for two reasons:
//   1) Some cards are tied to a specific moment (e.g. Weather Control fires
//      only at BeforeStormMove).
//   2) Tleilaxu Ghola plays "at any time" but only at deterministic safe
//      checkpoints (between phases, between players within a phase, after
//      BATTLE_RESOLVED / LEADER_KILLED) — those checkpoints raise an
//      AnytimeSafe window.
//
// PR 3 wires Weather Control, Hajr, Harkonnen capture, BG Voice, Atreides
// peek, and Tleilaxu Ghola through these windows. PR 4 adds revival hooks.
enum class ReactionWindow {
	BeforeStormMove,
	AfterStormMove,
	BeforeShipment,
	AfterShipment,
	BeforeMovement,
	AfterMovement,
	BeforeBattlePlanReveal,
	AfterBattlePlanReveal,
	BeforeBattleResolution,
	AfterBattleResolution,
	BeforeRevival,
	AfterRevival,
	AnytimeSafe,
	None,
};

const char* reactionWindowName(ReactionWindow w);
