#include "doctest/doctest.h"
#include "events/game_event_bus.hpp"
#include "events/event.hpp"
#include "logger/bus_bridge_logger.hpp"
#include "logger/event_logger.hpp"
#include <vector>
#include <cstdint>
#include <memory>

// Helper: make a simple Event with a given type and turn number.
static Event makeEvent(EventType type, int turn = 1, const std::string& phase = "STORM") {
	return Event(type, "test", turn, phase);
}

TEST_SUITE("GameEventBus") {

TEST_CASE("event_id is monotonically increasing from 1") {
	GameEventBus bus;
	for (int i = 0; i < 100; ++i) {
		uint64_t id = bus.publish(makeEvent(EventType::DEBUG_INFO, i));
		CHECK(id == static_cast<uint64_t>(i + 1));
	}
}

TEST_CASE("history grows in publish order") {
	GameEventBus bus;
	constexpr int N = 50;
	for (int i = 0; i < N; ++i) {
		bus.publish(makeEvent(EventType::DEBUG_INFO, i));
	}
	const auto& hist = bus.history();
	REQUIRE(hist.size() == N);
	for (int i = 0; i < N; ++i) {
		CHECK(hist[i].event_id == static_cast<uint64_t>(i + 1));
		CHECK(hist[i].event.turnNumber == i);
	}
}

TEST_CASE("tick increases with every publish") {
	GameEventBus bus;
	for (int i = 1; i <= 20; ++i) {
		bus.publish(makeEvent(EventType::DEBUG_INFO));
		CHECK(bus.currentTick() == static_cast<uint64_t>(i));
	}
}

TEST_CASE("subscriber is called synchronously and in subscribe order") {
	GameEventBus bus;
	std::vector<int> call_order;
	bus.subscribe([&](const GameEvent&) { call_order.push_back(1); });
	bus.subscribe([&](const GameEvent&) { call_order.push_back(2); });
	bus.subscribe([&](const GameEvent&) { call_order.push_back(3); });

	bus.publish(makeEvent(EventType::TURN_STARTED));

	REQUIRE(call_order.size() == 3);
	CHECK(call_order[0] == 1);
	CHECK(call_order[1] == 2);
	CHECK(call_order[2] == 3);
}

TEST_CASE("subscriber receives the published GameEvent with correct id") {
	GameEventBus bus;
	uint64_t received_id = 0;
	int received_turn = -1;
	bus.subscribe([&](const GameEvent& ge) {
		received_id = ge.event_id;
		received_turn = ge.event.turnNumber;
	});

	bus.publish(makeEvent(EventType::TURN_STARTED, 7));

	CHECK(received_id == 1);
	CHECK(received_turn == 7);
}

TEST_CASE("unsubscribe stops further notifications") {
	GameEventBus bus;
	int count = 0;
	auto id = bus.subscribe([&](const GameEvent&) { ++count; });

	bus.publish(makeEvent(EventType::DEBUG_INFO));
	CHECK(count == 1);

	bus.unsubscribe(id);
	bus.publish(makeEvent(EventType::DEBUG_INFO));
	bus.publish(makeEvent(EventType::DEBUG_INFO));
	CHECK(count == 1);  // no new calls after unsubscribe
}

TEST_CASE("clearHistory resets event_id, tick, and history") {
	GameEventBus bus;
	bus.publish(makeEvent(EventType::DEBUG_INFO));
	bus.publish(makeEvent(EventType::DEBUG_INFO));
	bus.clearHistory();

	CHECK(bus.history().empty());
	CHECK(bus.currentTick() == 0);

	uint64_t id = bus.publish(makeEvent(EventType::DEBUG_INFO));
	CHECK(id == 1);
	CHECK(bus.history().size() == 1);
}

TEST_CASE("legacy Event convenience overload wraps correctly") {
	GameEventBus bus;
	Event e = makeEvent(EventType::PHASE_STARTED, 3, "BATTLE");
	uint64_t id = bus.publish(e);

	REQUIRE(bus.history().size() == 1);
	const GameEvent& ge = bus.history().front();
	CHECK(ge.event_id == id);
	CHECK(ge.event.type == EventType::PHASE_STARTED);
	CHECK(ge.event.turnNumber == 3);
	CHECK(ge.event.currentPhase == "BATTLE");
	CHECK(ge.correlation_id == 0);
}

TEST_CASE("BusBridgeLogger publishes on logEvent and forwards to inner") {
	struct RecordingLogger : public EventLogger {
		int event_count = 0;
		void logEvent(const Event&) override { ++event_count; }
		void logError(const std::string&) override {}
		void logDebug(const std::string&) override {}
	};

	GameEventBus bus;
	auto* rec = new RecordingLogger();
	BusBridgeLogger bridge(std::unique_ptr<EventLogger>(rec), bus);

	Event e = makeEvent(EventType::BATTLE_STARTED, 2);
	bridge.logEvent(e);
	bridge.logEvent(e);

	CHECK(rec->event_count == 2);        // inner received both
	CHECK(bus.history().size() == 2);    // bus got both
}

}  // TEST_SUITE("GameEventBus")
