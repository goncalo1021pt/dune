// event_recorder.hpp — capture an ordered event stream from a GameEventBus
// for replay / determinism checks (Phase 5).
//
// Lifecycle:
//   GameEventBus bus;
//   EventRecorder rec;
//   rec.attach(bus);          // starts subscribing; new events accumulate
//   ... game runs ...
//   std::string log = rec.dumpJsonLines();
//   rec.detach();              // unsubscribe
//
// The recorder doesn't take ownership of the bus — it just holds a
// SubscriptionId and the bus reference for as long as it's attached.
// Detaching is safe to skip: ~EventRecorder calls detach() automatically.

#pragma once

#include "game_event_bus.hpp"
#include "game_event.hpp"

#include <string>
#include <vector>

class EventRecorder {
public:
	EventRecorder() = default;
	~EventRecorder();

	EventRecorder(const EventRecorder&) = delete;
	EventRecorder& operator=(const EventRecorder&) = delete;

	// Subscribe to `bus` and start capturing. Subsequent publishes land in
	// the recorder's buffer until detach() (or destruction). Calling attach
	// while already attached is a no-op (returns false).
	bool attach(GameEventBus& bus);

	// Stop capturing. Safe to call multiple times.
	void detach();

	// Read-only access to the captured events in publish order.
	const std::vector<GameEvent>& events() const { return events_; }
	std::size_t eventCount() const { return events_.size(); }

	// Serialize the captured events to a JSON-lines string (one JSON-encoded
	// Event per line; trailing newline). Reuses EventSerialization::toJson
	// so the format matches the FFI v2 event shape exactly.
	std::string dumpJsonLines() const;

	// Drop all captured events without detaching from the bus.
	void clear() { events_.clear(); }

private:
	GameEventBus* bus_ = nullptr;
	GameEventBus::SubscriptionId subscription_id_ = 0;
	std::vector<GameEvent> events_;
};
