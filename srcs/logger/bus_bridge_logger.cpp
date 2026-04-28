#include "logger/bus_bridge_logger.hpp"

BusBridgeLogger::BusBridgeLogger(std::unique_ptr<EventLogger> inner, GameEventBus& bus)
	: inner_(std::move(inner)), bus_(bus) {}

void BusBridgeLogger::logEvent(const Event& event) {
	bus_.publish(event);
	if (inner_) {
		inner_->logEvent(event);
	}
}

void BusBridgeLogger::logError(const std::string& message) {
	if (inner_) {
		inner_->logError(message);
	}
}

void BusBridgeLogger::logDebug(const std::string& message) {
	if (inner_) {
		inner_->logDebug(message);
	}
}
