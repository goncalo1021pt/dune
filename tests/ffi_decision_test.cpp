// Tests for the FFI v2 decision serialization helpers (PR 4b commit 1).
// These lock in the JSON shape Godot will read/write at the C ABI boundary.

#include "doctest/doctest.h"
#include "ffi/decision_serialization.hpp"
#include "interaction/interaction_adapter.hpp"

#include <nlohmann/json.hpp>
#include <set>
#include <string>

using json = nlohmann::json;
using DecisionSerialization::requestToJson;
using DecisionSerialization::parseSubmitJson;

TEST_SUITE("DecisionSerialization") {

TEST_CASE("requestToJson serialises a yn request with all standard fields") {
	DecisionRequest req;
	req.correlation_id = 7;
	req.kind = "yn";
	req.actor_index = 2;
	req.prompt = "Play Weather Control?";

	json j = json::parse(requestToJson(req));
	CHECK(j["correlation_id"].get<uint64_t>() == 7u);
	CHECK(j["kind"].get<std::string>() == "yn");
	CHECK(j["actor_index"].get<int>() == 2);
	CHECK(j["prompt"].get<std::string>() == "Play Weather Control?");
	CHECK(j["options"].is_array());
	CHECK(j["options"].empty());
	CHECK(j["allow_none"].get<bool>() == false);
	CHECK(j["int_min"].get<int>() == 0);
	CHECK(j["int_max"].get<int>() == 0);
}

TEST_CASE("requestToJson includes options for select kind") {
	DecisionRequest req;
	req.kind = "select";
	req.actor_index = 0;
	req.prompt = "Pick a leader:";
	req.options = {"Thufir Hawat", "Gurney Halleck", "Duncan Idaho"};
	req.allow_none = false;

	json j = json::parse(requestToJson(req));
	REQUIRE(j["options"].is_array());
	REQUIRE(j["options"].size() == 3);
	CHECK(j["options"][0].get<std::string>() == "Thufir Hawat");
	CHECK(j["options"][2].get<std::string>() == "Duncan Idaho");
}

TEST_CASE("requestToJson includes int_min/int_max for int kind") {
	DecisionRequest req;
	req.kind = "int";
	req.actor_index = 4;
	req.prompt = "How many spice to bid (0-12)?";
	req.int_min = 0;
	req.int_max = 12;

	json j = json::parse(requestToJson(req));
	CHECK(j["int_min"].get<int>() == 0);
	CHECK(j["int_max"].get<int>() == 12);
}

TEST_CASE("requestToJson emits only the documented fields") {
	// Lock the v2 schema down: the JSON we hand the host must contain
	// exactly the fields documented in snapshot_schema.md and nothing
	// extra. PR 4c removed migration_ctx from DecisionRequest entirely;
	// this is the regression check that no one re-adds an unsafe field.
	DecisionRequest req;
	req.kind = "select";
	req.actor_index = 1;
	req.prompt = "Pick:";
	req.options = {"a", "b"};

	json j = json::parse(requestToJson(req));
	const std::set<std::string> expected = {
		"correlation_id", "kind", "actor_index", "prompt",
		"options", "allow_none", "int_min", "int_max"
	};
	std::set<std::string> actual;
	for (auto it = j.begin(); it != j.end(); ++it) actual.insert(it.key());
	CHECK(actual == expected);
}

TEST_CASE("parseSubmitJson reads a simple-kind value") {
	auto resp = parseSubmitJson(R"({"value":"y"})");
	REQUIRE(resp.has_value());
	CHECK(resp->valid);
	CHECK(resp->payload_json == "y");
	CHECK(resp->correlation_id == 0u);
}

TEST_CASE("parseSubmitJson preserves correlation_id when present") {
	auto resp = parseSubmitJson(R"({"value":"5","correlation_id":42})");
	REQUIRE(resp.has_value());
	CHECK(resp->payload_json == "5");
	CHECK(resp->correlation_id == 42u);
}

TEST_CASE("parseSubmitJson stuffs the value string verbatim into payload_json") {
	// The FFI never parses the value string — the engine's existing per-kind
	// parsing consumes payload_json directly. This test pins that contract:
	// whatever the host puts in "value", that's what the engine gets, byte
	// for byte. Quotes and embedded JSON in particular must not be touched.
	const std::string raw = R"({"x":42,"q":"\"hi\""})";
	json wrapped = {{"value", raw}};
	auto resp = parseSubmitJson(wrapped.dump());
	REQUIRE(resp.has_value());
	CHECK(resp->payload_json == raw);
}

TEST_CASE("parseSubmitJson rejects malformed input") {
	CHECK_FALSE(parseSubmitJson("").has_value());
	CHECK_FALSE(parseSubmitJson("not json").has_value());
	CHECK_FALSE(parseSubmitJson("[1,2,3]").has_value());        // not an object
	CHECK_FALSE(parseSubmitJson("{}").has_value());              // missing value
	CHECK_FALSE(parseSubmitJson(R"({"value":42})").has_value()); // value not a string
	CHECK_FALSE(parseSubmitJson(R"({"other":"y"})").has_value()); // wrong key
}

TEST_CASE("requestToJson round-trips through nlohmann parser cleanly") {
	// Locks in that the produced string is always valid JSON. A locale leak
	// or unescaped character would surface here.
	DecisionRequest req;
	req.kind = "select";
	req.actor_index = 3;
	req.prompt = "Choose: \"foo\"\nor\tbar?";  // quotes, newline, tab
	req.options = {"a\\b", "c\"d"};

	json j;
	CHECK_NOTHROW(j = json::parse(requestToJson(req)));
	CHECK(j["prompt"].get<std::string>() == "Choose: \"foo\"\nor\tbar?");
	CHECK(j["options"][0].get<std::string>() == "a\\b");
	CHECK(j["options"][1].get<std::string>() == "c\"d");
}

} // TEST_SUITE
