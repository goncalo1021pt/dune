// Decision-trace replay test (Phase 5 commit 3). Lands the second half
// of acceptance criterion 6: "Deterministic replay passes for fixed
// seed + decision trace."
//
// Flow:
//   1. Run seed=42 in interactive mode wrapping a deterministic policy
//      with a RecordingAdapter. Capture both the event stream (via
//      EventRecorder) and the decision trace.
//   2. Run seed=42 again with a ScriptedAdapter fed the captured trace.
//      Capture the second event stream.
//   3. Assert: both streams are identical.
//
// If anything diverges — RNG consumed in a different order, decision
// kinds emitted in a different sequence, hidden non-determinism — the
// streams disagree and this fails. Combined with the AI determinism
// tests in determinism_test.cpp, this pins criterion 6 closed.

#include "doctest/doctest.h"
#include "events/event_recorder.hpp"
#include "interaction/interaction_adapter.hpp"
#include "interaction/recording_adapter.hpp"
#include "interaction/scripted_adapter.hpp"
#include "game.hpp"

#include <memory>
#include <string>

namespace {

// Deterministic test policy: matches the C smoke's craft_response logic
// (yn → "n", int → int_min, select → first option or "" if allow_none).
// Pure synchronous adapter — never yields. Suitable for both wrapping
// in a RecordingAdapter (run 1) and as the inner policy that drives the
// recording.
class DeterministicPolicyAdapter : public IInteractionAdapter {
public:
	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override {
		DecisionResponse resp;
		resp.correlation_id = req.correlation_id;
		resp.valid = true;
		if (req.kind == "yn") {
			resp.payload_json = "n";
		} else if (req.kind == "int") {
			resp.payload_json = std::to_string(req.int_min);
		} else if (req.kind == "select") {
			if (req.allow_none || req.options.empty()) {
				resp.payload_json = "";
			} else {
				resp.payload_json = req.options[0];
			}
		} else {
			resp.payload_json = "";
		}
		return resp;
	}
};

} // namespace

TEST_SUITE("Replay") {

TEST_CASE("interactive replay reproduces the recorded event stream byte-for-byte") {
	// --- Run 1: drive with deterministic policy, recording the trace. ---
	DeterministicPolicyAdapter policy;
	auto recorder = std::make_unique<RecordingAdapter>(policy);
	RecordingAdapter* recorder_ptr = recorder.get();  // non-owning for trace pull

	Game gameA(6, /*seed=*/42, /*interactive=*/true);
	gameA.setInteractionAdapter(std::move(recorder));
	EventRecorder eventRecA;
	eventRecA.attach(gameA.getEventBus());
	gameA.runGame();
	std::string streamA = eventRecA.dumpJsonLines();

	auto trace = recorder_ptr->trace();  // copy out before gameA's destructor
	REQUIRE(trace.size() > 0);
	CHECK(streamA.size() > 0);

	// --- Run 2: replay the trace via ScriptedAdapter. ---
	auto script = std::make_unique<ScriptedAdapter>(trace);
	ScriptedAdapter* script_ptr = script.get();

	Game gameB(6, /*seed=*/42, /*interactive=*/true);
	gameB.setInteractionAdapter(std::move(script));
	EventRecorder eventRecB;
	eventRecB.attach(gameB.getEventBus());
	gameB.runGame();
	std::string streamB = eventRecB.dumpJsonLines();

	// --- Assertions. ---
	CHECK(streamA == streamB);
	CHECK_MESSAGE(script_ptr->inSync(),
		"ScriptedAdapter detected kind/actor drift during replay");
	// Replay should consume exactly the recorded number of decisions; if
	// it consumed fewer, the engine took a shorter path; if more, it
	// asked beyond the trace and we'd have hit the out-of-sync branch.
	CHECK(script_ptr->consumed() == trace.size());
}

TEST_CASE("ScriptedAdapter detects kind drift") {
	// Lock in the diagnostic: if the live engine asks for a different
	// kind than the trace expected, inSync() flips false. We fabricate
	// a one-entry trace with kind="yn" and call requestDecision with
	// kind="int" to verify the detection.
	DecisionTraceEntry entry;
	entry.request.kind = "yn";
	entry.request.actor_index = 0;
	entry.response.payload_json = "n";
	entry.response.valid = true;

	ScriptedAdapter script({entry});
	DecisionRequest mismatched;
	mismatched.kind = "int";
	mismatched.actor_index = 0;
	auto resp = script.requestDecision(mismatched);
	REQUIRE(resp.has_value());
	CHECK_FALSE(script.inSync());
}

TEST_CASE("ScriptedAdapter goes out-of-sync when the trace runs short") {
	// Engine asks for more decisions than the trace has → inSync() flips
	// false and the response carries valid=false so the engine can degrade
	// gracefully instead of crashing.
	ScriptedAdapter empty(std::vector<DecisionTraceEntry>{});
	DecisionRequest req;
	req.kind = "yn";
	auto resp = empty.requestDecision(req);
	REQUIRE(resp.has_value());
	CHECK_FALSE(resp->valid);
	CHECK_FALSE(empty.inSync());
}

TEST_CASE("RecordingAdapter forwards request/response pairs into the trace in order") {
	DeterministicPolicyAdapter policy;
	RecordingAdapter rec(policy);

	DecisionRequest r1;
	r1.kind = "yn";
	r1.actor_index = 2;
	auto resp1 = rec.requestDecision(r1);

	DecisionRequest r2;
	r2.kind = "select";
	r2.actor_index = 4;
	r2.options = {"a", "b"};
	auto resp2 = rec.requestDecision(r2);

	REQUIRE(resp1.has_value());
	REQUIRE(resp2.has_value());
	CHECK(resp1->payload_json == "n");
	CHECK(resp2->payload_json == "a");

	const auto& t = rec.trace();
	REQUIRE(t.size() == 2);
	CHECK(t[0].request.kind == "yn");
	CHECK(t[0].response.payload_json == "n");
	CHECK(t[1].request.kind == "select");
	CHECK(t[1].response.payload_json == "a");
}

} // TEST_SUITE
