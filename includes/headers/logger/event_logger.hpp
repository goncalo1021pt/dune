#pragma once

#include "../events/event.hpp"
#include <string>

// Abstract EventLogger interface for game event logging
class EventLogger {
public:
	virtual ~EventLogger() = default;
	
	// Log a game event
	virtual void logEvent(const Event& event) = 0;
	
	// Log an error message
	virtual void logError(const std::string& message) = 0;
	
	// Log a simple debug message
	virtual void logDebug(const std::string& message) = 0;
};
