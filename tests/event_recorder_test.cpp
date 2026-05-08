// Tests for EventRecorder (Phase 5 commit 1). Locks in: the recorder
// captures events in publish order, dumps them as JSON-lines, and
// detach() / destruction both unsubscribe cleanly.

#include "doctest/doctest.h"
#include "events/event_recorder.hpp"
#include "events/game_event_bus.hpp"
#include "events/event.hpp"

#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

Event makeEvent(EventType type, const std::string& msg, int turn) {
	Event e(type, msg, turn, "TEST");
	return e;
}

std::vector<std::string> splitLines(const std::string& s) {
	std::vector<std::string> lines;
	std::stringstream ss(s);
	std::string line;
	while (std::getline(ss, line)) lines.push_back(line);
	return lines;
}

} // namespace

TEST_SUITE("EventRecorder") {

TEST_CASE("attach captures published events in publish order") {
	GameEventBus bus;
	EventRecorder rec;
	REQUIRE(rec.attach(bus));

	bus.publish(makeEvent(EventType::TURN_STARTED, "turn 1", 1));
	bus.publish(makeEvent(EventType::PHASE_STARTED, "STORM", 1));
	bus.publish(makeEvent(EventType::STORM_MOVED, "storm moves", 1));

	CHECK(rec.eventCount() == 3);
	const auto& captured = rec.events();
	CHECK(captured[0].event.type == EventType::TURN_STARTED);
	CHECK(captured[1].event.type == EventType::PHASE_STARTED);
	CHECK(captured[2].event.type == EventType::STORM_MOVED);
	// Bus assigns monotonic event_id; recorder preserves it.
	CHECK(captured[0].event_id == 1u);
	CHECK(captured[1].event_id == 2u);
	CHECK(captured[2].event_id == 3u);
}

TEST_CASE("events published before attach are not captured") {
	GameEventBus bus;
	bus.publish(makeEvent(EventType::TURN_STARTED, "before", 0));

	EventRecorder rec;
	REQUIRE(rec.attach(bus));
	bus.publish(makeEvent(EventType::TURN_STARTED, "after", 1));

	CHECK(rec.eventCount() == 1);
	CHECK(rec.events()[0].event.message == "after");
}

TEST_CASE("attach is a no-op when already attached") {
	GameEventBus bus;
	EventRecorder rec;
	REQUIRE(rec.attach(bus));
	CHECK_FALSE(rec.attach(bus));  // second call returns false

	bus.publish(makeEvent(EventType::DEBUG_INFO, "x", 0));
	// Was the recorder subscribed once or twice? If twice, we'd see the event
	// duplicated in events(). Once is correct.
	CHECK(rec.eventCount() == 1);
}

TEST_CASE("detach unsubscribes; subsequent publishes are not captured") {
	GameEventBus bus;
	EventRecorder rec;
	REQUIRE(rec.attach(bus));
	bus.publish(makeEvent(EventType::TURN_STARTED, "captured", 1));

	rec.detach();
	bus.publish(makeEvent(EventType::TURN_ENDED, "after detach", 1));

	CHECK(rec.eventCount() == 1);
	CHECK(rec.events()[0].event.message == "captured");
}

TEST_CASE("destruction detaches even if detach() was not called") {
	GameEventBus bus;
	{
		EventRecorder rec;
		REQUIRE(rec.attach(bus));
		bus.publish(makeEvent(EventType::TURN_STARTED, "x", 1));
		// rec goes out of scope here without explicit detach
	}
	CHECK(bus.subscriberCount() == 0);
	// And bus is still usable after the recorder dies.
	bus.publish(makeEvent(EventType::TURN_ENDED, "y", 1));
}

TEST_CASE("dumpJsonLines produces one valid JSON object per line in publish order") {
	GameEventBus bus;
	EventRecorder rec;
	REQUIRE(rec.attach(bus));

	bus.publish(makeEvent(EventType::TURN_STARTED, "first", 1));
	bus.publish(makeEvent(EventType::PHASE_STARTED, "second", 1));

	std::string dump = rec.dumpJsonLines();
	auto lines = splitLines(dump);
	REQUIRE(lines.size() == 2);

	json j0 = json::parse(lines[0]);
	json j1 = json::parse(lines[1]);
	CHECK(j0["type"].get<std::string>() == "TURN_STARTED");
	CHECK(j0["message"].get<std::string>() == "first");
	CHECK(j1["type"].get<std::string>() == "PHASE_STARTED");
	CHECK(j1["message"].get<std::string>() == "second");
}

TEST_CASE("clear empties the buffer but keeps the subscription alive") {
	GameEventBus bus;
	EventRecorder rec;
	REQUIRE(rec.attach(bus));
	bus.publish(makeEvent(EventType::TURN_STARTED, "x", 1));
	CHECK(rec.eventCount() == 1);

	rec.clear();
	CHECK(rec.eventCount() == 0);

	bus.publish(makeEvent(EventType::TURN_ENDED, "y", 1));
	CHECK(rec.eventCount() == 1);  // still subscribed; new event captured
}

} // TEST_SUITE
