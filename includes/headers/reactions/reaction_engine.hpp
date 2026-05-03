#pragma once

#include <string>
#include "reactions/reaction_window.hpp"

struct PhaseContext;

// ReactionEngine consolidates the ad-hoc reaction sites scattered across
// phase code under a uniform window-dispatch model.
//
// Each dispatch method:
//   1. Opens the named window (records currentWindow_, emits a debug log).
//   2. Iterates turn order and offers eligible factions a chance to react.
//   3. Closes the window before returning.
//
// State machines that spread across multiple decisions (BG Voice, Atreides
// peek) keep their existing internals in battle_phase.cpp; the engine just
// brackets them so the bus and legality checks observe the window.
//
// Legality:
//   isReactionLegalNow(cardName) returns true iff the card's reaction window
//   is currently open. Outside any window, all reaction-card plays are
//   illegal. This is the contract the test suite locks in.
class ReactionEngine {
public:
	ReactionEngine() = default;

	ReactionWindow currentWindow() const { return currentWindow_; }
	bool isWindowOpen() const { return currentWindow_ != ReactionWindow::None; }

	// Returns true if a reaction card with this name can legally be played
	// in the currently-open window. Unknown card names default to false.
	bool isReactionLegalNow(const std::string& cardName) const;

	// ---- Window dispatchers ----

	// Storm phase, before storm moves. Iterates turn order, offers each
	// "Weather Control" holder the chance to override the storm card.
	// Returns the resolved storm move (defaultMove if no one reacted).
	// Sets weatherControllerOut to the reactor's player index, or -1.
	int dispatchBeforeStormMove(PhaseContext& ctx, int defaultMove,
		int& weatherControllerOut);

	// Movement phase, after a player completes a regular movement. If they
	// hold "Hajr", asks whether to play it. Returns true iff Hajr was
	// played (caller then runs the second movement decision separately —
	// movement is a compound interaction we don't model inside the engine).
	bool dispatchAfterMovementHajr(PhaseContext& ctx, int playerIndex);

	// Battle phase, after battle resolution. Currently dispatches the
	// winner's onBattleWon hook (Harkonnen capture). Wrapped here so the
	// bus and legality checks see the window.
	void dispatchAfterBattleResolution(PhaseContext& ctx, int winnerIdx,
		int loserIdx);

	// Battle phase, before either side reveals their plan. The internal
	// state machines for BG Voice and Atreides peek live in battle_phase.cpp
	// (BeneVoiceState / AtreidesPeekState); this method only brackets them
	// so the window is observable.
	void dispatchBeforeBattlePlanReveal(PhaseContext& ctx, int attackerIdx,
		int defenderIdx);

	// AnytimeSafe checkpoint: between phases, between players in a phase,
	// after BATTLE_RESOLVED / LEADER_KILLED. Currently implements legality
	// only — Tleilaxu Ghola revival mechanics are tracked as a follow-up.
	// The window is opened only when at least one player still holds the
	// "Tleilaxu Ghola" card, to keep AI runs free of pointless prompts.
	void dispatchAnytimeSafe(PhaseContext& ctx, const std::string& checkpointLabel);

	// RAII helper: enters a window for the duration of a scope. Restores
	// the previous window state on exit so nested dispatches behave
	// correctly (e.g., AnytimeSafe inside AfterBattleResolution). Public
	// so tests can stage legality checks without a full PhaseContext.
	class WindowGuard {
	public:
		WindowGuard(ReactionEngine& engine, ReactionWindow w)
			: engine_(engine), prev_(engine.currentWindow_) {
			engine_.currentWindow_ = w;
		}
		~WindowGuard() { engine_.currentWindow_ = prev_; }
		WindowGuard(const WindowGuard&) = delete;
		WindowGuard& operator=(const WindowGuard&) = delete;
	private:
		ReactionEngine& engine_;
		ReactionWindow prev_;
	};

private:
	ReactionWindow currentWindow_ = ReactionWindow::None;

	// Logs REACTION_WINDOW_OPENED through ctx.logger when set.
	void logWindowOpen(PhaseContext& ctx, ReactionWindow w,
		const std::string& detail) const;
};
