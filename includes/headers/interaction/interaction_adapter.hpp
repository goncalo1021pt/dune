#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Decision request issued by the engine when a player choice is needed.
struct DecisionRequest {
	uint64_t correlation_id = 0;
	std::string kind;          // "yn", "int", "select", "deployment", "movement"
	int actor_index = -1;      // player index; -1 if N/A

	// Human-readable prompt shown to the player.
	std::string prompt;

	// For "select" kind: ordered list of choices.
	std::vector<std::string> options;
	// For "select": whether choosing nothing (0) is legal.
	bool allow_none = false;

	// For "int" kind: inclusive range.
	int int_min = 0;
	int int_max = 0;

	// Structured context for future FFI use (PR 4). Not yet used in-process.
	std::string payload_json;

	// Temporary migration field: raw pointer to PhaseContext for compound
	// decisions (deployment, movement) that still need full game state.
	// REMOVED in PR 4 once compound payloads are fully serialized.
	void* migration_ctx = nullptr;
};

// Response from the adapter (sync) or from the FFI host (async).
// payload_json carries the chosen value:
//   "yn"         -> "y" or "n"
//   "int"        -> decimal integer as string, e.g. "5"
//   "select"     -> chosen option string, or "" if allow_none and 0 selected
//   "deployment" -> JSON: {"territory":"...","normal":N,"elite":N,"sector":N,"skip":bool}
//   "movement"   -> JSON: {"from":"...","to":"...","normal":N,"elite":N,
//                          "from_sector":N,"to_sector":N,"skip":bool}
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
