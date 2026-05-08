#include "events/event_recorder.hpp"
#include "events/event_serialization.hpp"

EventRecorder::~EventRecorder() {
	detach();
}

bool EventRecorder::attach(GameEventBus& bus) {
	if (bus_ != nullptr) return false;  // already attached
	bus_ = &bus;
	subscription_id_ = bus.subscribe([this](const GameEvent& e) {
		events_.push_back(e);
	});
	return true;
}

void EventRecorder::detach() {
	if (bus_ == nullptr) return;
	if (subscription_id_ != 0) {
		bus_->unsubscribe(subscription_id_);
	}
	bus_ = nullptr;
	subscription_id_ = 0;
}

std::string EventRecorder::dumpJsonLines() const {
	std::string out;
	out.reserve(events_.size() * 96);  // rough average per event
	for (const auto& ge : events_) {
		out += EventSerialization::toJson(ge.event);
		out += '\n';
	}
	return out;
}
