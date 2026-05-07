#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Decision request issued by the engine when a player choice is needed.
//
// PR 4c: only primitive kinds remain. The engine drives multi-step flows
// (deployment, movement) by issuing sequences of these primitives — no
// adapter has to know about the compound shape any more.
struct DecisionRequest {
	uint64_t correlation_id = 0;
	std::string kind;          // "yn", "int", "select"
	int actor_index = -1;      // player index; -1 if N/A

	// Human-readable prompt shown to the player.
	std::string prompt;

	// For "select" kind: ordered list of choices.
	std::vector<std::string> options;
	// For "select": whether choosing nothing (empty value) is legal.
	bool allow_none = false;

	// For "int" kind: inclusive range.
	int int_min = 0;
	int int_max = 0;
};

// Response from the adapter (sync) or from the FFI host (async).
// payload_json carries the chosen value, by kind:
//   "yn"     -> "y" or "n"
//   "int"    -> decimal integer as string, e.g. "5"
//   "select" -> chosen option string, or "" if allow_none and the player skips
struct DecisionResponse {
	uint64_t correlation_id = 0;
	std::string payload_json;
	bool valid = true;
};

// IInteractionAdapter: contract between the engine and any client that drives decisions.
//
// Sync (TtyAdapter, PR 2): reads from stdin, returns immediately.
// Async (Godot/FFI adapter, PR 4): returns std::nullopt to signal "yield — host will
//   call dune_session_submit_decision() before the next step".
class IInteractionAdapter {
public:
	virtual ~IInteractionAdapter() = default;

	virtual std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) = 0;
};
