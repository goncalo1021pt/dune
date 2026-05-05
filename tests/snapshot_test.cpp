#include "doctest/doctest.h"
#include "ffi/snapshot_builder.hpp"
#include "events/event_serialization.hpp"
#include "events/event.hpp"
#include "game.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// SnapshotBuilder produces a JSON document that the Godot frontend / future
// server depends on. These tests lock in the v1 schema so adding new fields
// stays additive — a removed/renamed field will fail one of these checks.

TEST_SUITE("SnapshotBuilder") {

TEST_CASE("snapshot of a freshly-initialised game has all required top-level keys") {
	Game g(6, /*seed=*/42, /*interactive=*/false);
	g.initializeGame();

	std::string snap = SnapshotBuilder::buildSnapshotJson(g);
	json j = json::parse(snap);

	CHECK(j.contains("schema_version"));
	CHECK(j["schema_version"].get<int>() == 1);
	CHECK(j.contains("game"));
	CHECK(j.contains("feature_settings"));
	CHECK(j.contains("storm"));
	CHECK(j.contains("players"));
	CHECK(j.contains("map"));
	CHECK(j.contains("treachery_discard"));
	CHECK(j.contains("reactions"));
}

TEST_CASE("snapshot game block reports turn 0 and STORM phase before runGame") {
	Game g(6, /*seed=*/42, /*interactive=*/false);
	g.initializeGame();

	json j = json::parse(SnapshotBuilder::buildSnapshotJson(g));

	CHECK(j["game"]["turn"].get<int>() == 0);
	CHECK(j["game"]["phase"].get<std::string>() == "STORM");
	CHECK(j["game"]["player_count"].get<int>() == 6);
	CHECK(j["game"]["initial_seed"].get<unsigned int>() == 42u);
	CHECK(j["game"]["game_ended"].get<bool>() == false);
	CHECK(j["game"]["interactive"].get<bool>() == false);
}

TEST_CASE("each player block contains required identity + resource fields") {
	Game g(6, /*seed=*/42, /*interactive=*/false);
	g.initializeGame();

	json j = json::parse(SnapshotBuilder::buildSnapshotJson(g));
	auto& players = j["players"];
	REQUIRE(players.is_array());
	REQUIRE(players.size() == 6);

	for (auto& p : players) {
		CHECK(p.contains("faction_index"));
		CHECK(p.contains("faction_name"));
		CHECK(p.contains("spice"));
		CHECK(p.contains("reserve"));
		CHECK(p.contains("deployed"));
		CHECK(p.contains("destroyed"));
		CHECK(p.contains("treachery_cards"));
		CHECK(p.contains("traitor_cards"));
		CHECK(p.contains("leaders_alive"));
		CHECK(p.contains("leaders_dead"));
		CHECK(p["faction_name"].get<std::string>().size() > 0);
	}
}

TEST_CASE("territory block lists units + spice with sector info") {
	Game g(6, /*seed=*/42, /*interactive=*/false);
	g.initializeGame();

	json j = json::parse(SnapshotBuilder::buildSnapshotJson(g));
	auto& territories = j["map"]["territories"];
	REQUIRE(territories.is_array());
	CHECK(territories.size() > 0);

	for (auto& t : territories) {
		CHECK(t.contains("name"));
		CHECK(t.contains("terrain"));
		CHECK(t.contains("sectors"));
		CHECK(t.contains("units"));
		CHECK(t.contains("spice"));
	}

	// At least one territory has units after initial placement (Atreides
	// in Arrakeen, Harkonnen in Carthag, Fremen in their sietches, etc).
	bool anyUnits = false;
	for (auto& t : territories) {
		if (!t["units"].empty()) { anyUnits = true; break; }
	}
	CHECK(anyUnits);
}

TEST_CASE("two concurrent sessions produce independent snapshots") {
	// Server-multiplayer scenario: one process hosts multiple games. The
	// snapshot for game A must not leak state from game B.
	Game a(6, /*seed=*/42, /*interactive=*/false);
	a.initializeGame();
	Game b(6, /*seed=*/99, /*interactive=*/false);
	b.initializeGame();

	std::string snapA = SnapshotBuilder::buildSnapshotJson(a);
	std::string snapB = SnapshotBuilder::buildSnapshotJson(b);
	json ja = json::parse(snapA);
	json jb = json::parse(snapB);

	CHECK(ja["game"]["initial_seed"].get<unsigned int>() == 42u);
	CHECK(jb["game"]["initial_seed"].get<unsigned int>() == 99u);

	// Different seeds shuffle decks differently, so the snapshots' raw
	// content must differ even before any phase has run. (The storm sector
	// itself stays 0 until the STORM phase places it on turn 1.)
	CHECK(snapA != snapB);
}

TEST_CASE("snapshot remains valid JSON after a full AI run") {
	Game g(6, /*seed=*/42, /*interactive=*/false);
	g.initializeGame();
	g.runGame();

	std::string snap = SnapshotBuilder::buildSnapshotJson(g);
	// Round-trip through nlohmann/json: if parse throws, the snapshot is
	// malformed (escape bug, locale leak, etc.).
	json j;
	CHECK_NOTHROW(j = json::parse(snap));
	CHECK(j["game"]["game_ended"].get<bool>() == true);
}

} // TEST_SUITE

TEST_SUITE("EventSerialization") {

TEST_CASE("Event with default visibility serialises as public") {
	Event e(EventType::STORM_MOVED, "test", 1, "STORM");
	json j = json::parse(EventSerialization::toJson(e));

	CHECK(j["type"].get<std::string>() == "STORM_MOVED");
	CHECK(j["visibility"].get<std::string>() == "public");
	CHECK(j["actor_faction"].get<int>() == -1);
}

TEST_CASE("Event marked PrivateToActor round-trips with the actor index") {
	Event e(EventType::BID_PLACED, "test", 2, "BIDDING");
	e.visibility = EventVisibility::PrivateToActor;
	e.actorFactionIndex = 3;

	json j = json::parse(EventSerialization::toJson(e));
	CHECK(j["visibility"].get<std::string>() == "private_to_actor");
	CHECK(j["actor_faction"].get<int>() == 3);
}

TEST_CASE("eventTypeName matches the enum") {
	CHECK(std::string(EventSerialization::eventTypeName(static_cast<int>(EventType::GAME_INITIALIZED))) == "GAME_INITIALIZED");
	CHECK(std::string(EventSerialization::eventTypeName(static_cast<int>(EventType::REACTION_WINDOW_OPENED))) == "REACTION_WINDOW_OPENED");
	CHECK(std::string(EventSerialization::eventTypeName(static_cast<int>(EventType::ERROR_EVENT))) == "ERROR_EVENT");
}

} // TEST_SUITE
