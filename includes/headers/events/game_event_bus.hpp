#pragma once

#include "game_event.hpp"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// GameEventBus: in-process publish/subscribe for engine systems.
// Owns the canonical event log (history_), assigns monotonic event_id and tick.
// Subscribers are notified synchronously in the order they subscribed.
class GameEventBus {
public:
	using Subscriber = std::function<void(const GameEvent&)>;
	using SubscriptionId = uint64_t;

	GameEventBus();

	// Publish an event. Bus assigns event_id (monotonic from 1) and tick.
	// Returns the assigned event_id.
	uint64_t publish(GameEvent event);
	// Convenience overload for migration from the old Event-only path.
	uint64_t publish(const Event& legacy_event);

	// Subscribe a callback. Returns a non-zero handle; pass it to unsubscribe.
	SubscriptionId subscribe(Subscriber subscriber);
	void unsubscribe(SubscriptionId id);

	// Read-only access to the event log for replay/snapshot/tests.
	const std::vector<GameEvent>& history() const { return history_; }

	// Stats
	uint64_t lastEventId() const { return next_event_id_ - 1; }
	uint64_t currentTick() const { return tick_; }
	std::size_t subscriberCount() const { return subscribers_.size(); }

	// Test/debug only: reset the bus to empty.
	void clearHistory();

private:
	std::vector<GameEvent> history_;
	std::unordered_map<SubscriptionId, Subscriber> subscribers_;
	std::vector<SubscriptionId> subscriber_order_;  // preserves subscribe order for deterministic dispatch
	uint64_t next_event_id_;
	uint64_t next_subscription_id_;
	uint64_t tick_;
};
