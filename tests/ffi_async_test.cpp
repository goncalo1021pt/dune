// Tests for FFIAsyncAdapter (PR 4b commit 2). The adapter bridges the
// engine's sync requestDecision contract to an async host via a worker
// thread + condvar. These tests stand up a DecisionChannel, spawn a fake
// "worker thread" that calls requestDecision, and play the host's role
// from the test thread to drive the protocol end-to-end.
//
// The full FFI surface that uses this adapter (dune_session_step / submit /
// destroy) is wired up in commit 3.

#include "doctest/doctest.h"
#include "ffi/ffi_async_adapter.hpp"
#include "interaction/interaction_adapter.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace FFIAsync;
using namespace std::chrono_literals;

namespace {

// Wait for the channel's state to enter `target`, with a timeout so a
// broken adapter doesn't hang the test runner forever. Returns true on
// success, false on timeout.
bool waitForState(DecisionChannel& channel, SessionState target,
                  std::chrono::milliseconds timeout = 2s) {
	std::unique_lock<std::mutex> lock(channel.mu);
	return channel.cv.wait_for(lock, timeout, [&] {
		return channel.state == target;
	});
}

// Submit a response from the host side. Mirrors what the FFI submit
// function will do once it's wired up in commit 3.
void hostSubmit(DecisionChannel& channel, const std::string& payload) {
	{
		std::lock_guard<std::mutex> lock(channel.mu);
		channel.pendingResponse.payload_json = payload;
		channel.pendingResponse.valid = true;
		channel.state = SessionState::Running;
	}
	channel.cv.notify_all();
}

// Mark cancel and notify, mirroring what dune_session_destroy will do.
void hostCancel(DecisionChannel& channel) {
	{
		std::lock_guard<std::mutex> lock(channel.mu);
		channel.cancelRequested = true;
	}
	channel.cv.notify_all();
}

} // namespace

TEST_SUITE("FFIAsyncAdapter") {

TEST_CASE("requestDecision blocks until host submits, returns the response") {
	DecisionChannel channel;
	FFIAsyncAdapter adapter(&channel);

	DecisionRequest req;
	req.kind = "yn";
	req.actor_index = 1;
	req.prompt = "Play Hajr?";

	// Worker: calls requestDecision and stashes the result.
	auto fut = std::async(std::launch::async, [&] {
		return adapter.requestDecision(req);
	});

	// Host: wait for the worker to enter AwaitingDecision, then submit.
	REQUIRE(waitForState(channel, SessionState::AwaitingDecision));
	{
		std::lock_guard<std::mutex> lock(channel.mu);
		// Pending request is observable to the host.
		CHECK(channel.pendingRequest.kind == "yn");
		CHECK(channel.pendingRequest.actor_index == 1);
		CHECK(channel.pendingRequest.prompt == "Play Hajr?");
	}
	hostSubmit(channel, "y");

	auto resp = fut.get();
	REQUIRE(resp.has_value());
	CHECK(resp->valid);
	CHECK(resp->payload_json == "y");
}

TEST_CASE("requestDecision throws SessionCancelled when cancel is set before the call") {
	DecisionChannel channel;
	channel.cancelRequested = true;
	FFIAsyncAdapter adapter(&channel);

	DecisionRequest req;
	req.kind = "yn";
	CHECK_THROWS_AS(adapter.requestDecision(req), SessionCancelled);
}

TEST_CASE("requestDecision throws SessionCancelled when cancel is requested while waiting") {
	DecisionChannel channel;
	FFIAsyncAdapter adapter(&channel);

	std::atomic<bool> threw{false};
	auto worker = std::thread([&] {
		DecisionRequest req;
		req.kind = "yn";
		try {
			adapter.requestDecision(req);
		} catch (const SessionCancelled&) {
			threw = true;
		}
	});

	REQUIRE(waitForState(channel, SessionState::AwaitingDecision));
	hostCancel(channel);

	worker.join();
	CHECK(threw.load());
}

TEST_CASE("multiple sequential requestDecision calls reset the response slot") {
	// If a stale response leaked from one call to the next, the second
	// requestDecision could spuriously return the first response without
	// the host actually submitting. Lock that out.
	DecisionChannel channel;
	FFIAsyncAdapter adapter(&channel);

	DecisionRequest req1;
	req1.kind = "yn";
	auto fut1 = std::async(std::launch::async, [&] { return adapter.requestDecision(req1); });
	REQUIRE(waitForState(channel, SessionState::AwaitingDecision));
	hostSubmit(channel, "y");
	auto r1 = fut1.get();
	REQUIRE(r1.has_value());
	CHECK(r1->payload_json == "y");

	// Inspect the channel under the lock — pendingResponse must have been
	// cleared so it can't leak into the next call.
	{
		std::lock_guard<std::mutex> lock(channel.mu);
		CHECK(channel.pendingResponse.payload_json.empty());
	}

	DecisionRequest req2;
	req2.kind = "select";
	req2.options = {"a", "b"};
	auto fut2 = std::async(std::launch::async, [&] { return adapter.requestDecision(req2); });
	REQUIRE(waitForState(channel, SessionState::AwaitingDecision));
	hostSubmit(channel, "b");
	auto r2 = fut2.get();
	REQUIRE(r2.has_value());
	CHECK(r2->payload_json == "b");
}

TEST_CASE("correlation_id flows through request -> pendingRequest and response.payload_json is verbatim") {
	// Locks in: the adapter does not mutate request fields on the way in,
	// and does not interpret payload_json on the way out — even values that
	// contain quotes or embedded JSON pass through untouched.
	DecisionChannel channel;
	FFIAsyncAdapter adapter(&channel);

	DecisionRequest req;
	req.kind = "select";
	req.actor_index = 2;
	req.correlation_id = 99;
	req.options = {"Arrakeen", "Sietch Tabr"};

	auto fut = std::async(std::launch::async, [&] { return adapter.requestDecision(req); });
	REQUIRE(waitForState(channel, SessionState::AwaitingDecision));

	{
		std::lock_guard<std::mutex> lock(channel.mu);
		CHECK(channel.pendingRequest.correlation_id == 99u);
		CHECK(channel.pendingRequest.kind == "select");
		CHECK(channel.pendingRequest.options.size() == 2);
	}

	// Verbatim passthrough — even a value containing quotes and embedded
	// JSON is preserved byte-for-byte.
	const std::string verbatim = R"({"x":42,"q":"\"hi\""})";
	hostSubmit(channel, verbatim);

	auto resp = fut.get();
	REQUIRE(resp.has_value());
	CHECK(resp->payload_json == verbatim);
}

} // TEST_SUITE
