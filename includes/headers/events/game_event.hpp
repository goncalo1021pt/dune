#pragma once

#include "event.hpp"
#include <cstdint>
#include <string>

// GameEvent wraps the existing rich Event struct with bus-managed metadata.
// PR 1: introduces the type without changing how existing Event consumers work.
// Later PRs will populate payload_json at the FFI boundary.
struct GameEvent {
	uint64_t event_id;        // monotonic, assigned by GameEventBus on publish
	uint64_t correlation_id;  // 0 if not a request/response
	uint64_t tick;             // engine-monotonic counter, assigned by bus
	Event event;               // existing rich event (type, message, turn, phase, faction, territory, ...)
	std::string payload_json;  // optional typed payload, populated only at FFI boundary

	GameEvent()
		: event_id(0), correlation_id(0), tick(0), event(), payload_json("") {}

	explicit GameEvent(const Event& e)
		: event_id(0), correlation_id(0), tick(0), event(e), payload_json("") {}

	GameEvent(const Event& e, uint64_t correlation)
		: event_id(0), correlation_id(correlation), tick(0), event(e), payload_json("") {}
};
