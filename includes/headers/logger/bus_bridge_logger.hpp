#pragma once

#include "event_logger.hpp"
#include "../events/game_event_bus.hpp"
#include <memory>

// BusBridgeLogger: dual-path logger used during the event-system migration.
//
// Wraps an inner EventLogger (typically ConsoleEventLogger). For each logEvent call:
//   1. Publishes a GameEvent on the bus (so history accumulates and bus subscribers fire).
//   2. Forwards to the inner logger (so console output remains identical to pre-bus behavior).
//
// In PR 4 of the event-system migration, all phase code switches to bus.publish() directly,
// the ConsoleEventLogger becomes a bus subscriber, and this decorator is removed.
class BusBridgeLogger : public EventLogger {
public:
	BusBridgeLogger(std::unique_ptr<EventLogger> inner, GameEventBus& bus);
	~BusBridgeLogger() override = default;

	void logEvent(const Event& event) override;
	void logError(const std::string& message) override;
	void logDebug(const std::string& message) override;

	EventLogger* inner() const { return inner_.get(); }

private:
	std::unique_ptr<EventLogger> inner_;
	GameEventBus& bus_;
};
