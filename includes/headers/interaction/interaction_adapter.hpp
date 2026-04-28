#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Decision request issued by the engine when a player choice is needed.
// PR 1: type only, not yet wired into phase code (that lands in PR 2).
struct DecisionRequest {
	uint64_t correlation_id = 0;
	std::string kind;          // e.g. "deployment", "battle_leader", "voice_target"
	int actor_index = -1;      // player index of the actor; -1 if N/A
	std::string payload_json;  // legal options + context, JSON-encoded
};

// Response delivered by the adapter (sync) or by the FFI host (async).
struct DecisionResponse {
	uint64_t correlation_id = 0;
	std::string payload_json;
	bool valid = true;
};

// IInteractionAdapter: contract between the engine and any host that drives decisions.
// - TtyAdapter (PR 2) blocks on std::cin and returns synchronously.
// - FFI/Godot adapter (PR 4) returns std::nullopt to signal "yield, host will submit later".
class IInteractionAdapter {
public:
	virtual ~IInteractionAdapter() = default;

	// Synchronous adapters return a populated DecisionResponse.
	// Async adapters return std::nullopt; the engine then yields control to the host
	// and the host fulfills the decision via dune_session_submit_decision().
	virtual std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) = 0;
};
