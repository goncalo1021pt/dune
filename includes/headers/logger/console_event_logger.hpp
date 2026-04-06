#pragma once

#include "event_logger.hpp"
#include <iostream>
#include <sstream>

// Console-based implementation of EventLogger
// Prints events to stdout - can be swapped with GodotEventLogger later
class ConsoleEventLogger : public EventLogger {
public:
	ConsoleEventLogger();
	virtual ~ConsoleEventLogger() = default;
	
	// Log a game event to stdout
	void logEvent(const Event& event) override;
	
	// Log an error message
	void logError(const std::string& message) override;
	
	// Log a simple debug message
	void logDebug(const std::string& message) override;

private:
	// Helper to format event type to string
	std::string eventTypeToString(EventType type) const;
	
	// Helper to format and display full event
	void displayEvent(const Event& event);
};
