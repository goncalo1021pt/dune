#include "events/game_event_bus.hpp"

#include <algorithm>

GameEventBus::GameEventBus()
	: history_(),
	  subscribers_(),
	  subscriber_order_(),
	  next_event_id_(1),
	  next_subscription_id_(1),
	  tick_(0) {}

uint64_t GameEventBus::publish(GameEvent event) {
	event.event_id = next_event_id_++;
	event.tick = ++tick_;
	history_.push_back(event);

	const GameEvent& published = history_.back();
	for (SubscriptionId id : subscriber_order_) {
		auto it = subscribers_.find(id);
		if (it != subscribers_.end()) {
			it->second(published);
		}
	}
	return published.event_id;
}

uint64_t GameEventBus::publish(const Event& legacy_event) {
	return publish(GameEvent(legacy_event));
}

GameEventBus::SubscriptionId GameEventBus::subscribe(Subscriber subscriber) {
	SubscriptionId id = next_subscription_id_++;
	subscribers_[id] = std::move(subscriber);
	subscriber_order_.push_back(id);
	return id;
}

void GameEventBus::unsubscribe(SubscriptionId id) {
	subscribers_.erase(id);
	subscriber_order_.erase(
		std::remove(subscriber_order_.begin(), subscriber_order_.end(), id),
		subscriber_order_.end());
}

void GameEventBus::clearHistory() {
	history_.clear();
	next_event_id_ = 1;
	tick_ = 0;
}
