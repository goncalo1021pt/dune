// dune_c_api.cpp — implementation of the v1 C ABI exposed by libdune.so.
//
// Every function body is wrapped in try/catch — exceptions must not cross
// the C boundary. On unexpected throw we return DUNE_ERR_INTERNAL and the
// session is left in whatever state the throw occurred in (caller's safest
// move is then to destroy the session).
//
// String returns are heap-allocated via new char[]; freed by dune_free.

#include "ffi/dune_c_api.h"
#include "ffi/snapshot_builder.hpp"
#include "events/event_serialization.hpp"
#include "events/game_event_bus.hpp"
#include "game.hpp"

#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <new>

namespace {

// Heap-allocate a copy of `s` so the caller can dune_free it. Uses operator
// new[] for symmetry with operator delete[] in dune_free. Returns nullptr
// on allocation failure (rare; we still return DUNE_OK because the call
// itself succeeded — callers should null-check anyway).
char* duplicateForCaller(const std::string& s) noexcept {
	try {
		char* buf = new char[s.size() + 1];
		std::memcpy(buf, s.data(), s.size());
		buf[s.size()] = '\0';
		return buf;
	} catch (const std::bad_alloc&) {
		return nullptr;
	}
}

} // namespace

// The session struct is defined here — opaque to the caller. Holding the
// engine alongside its bus subscription so events emitted during run_to_end
// land in event_queue_ and can be drained by poll_event later.
struct dune_session {
	std::unique_ptr<Game> game;
	std::deque<GameEvent> event_queue;
	std::mutex queue_mutex;  // protects event_queue against the bus callback
	GameEventBus::SubscriptionId subscription_id = 0;
	bool ran_to_end = false;
};

extern "C" {

const char* dune_api_version(void) {
	return "1.0+pr4a";
}

const char* dune_api_build_info(void) {
	return "compiler=g++ std=c++17 schema_version=1 ffi_version=1.0+pr4a";
}

dune_session_t* dune_session_create(unsigned int seed, int num_players) {
	if (num_players < 2 || num_players > 6) return nullptr;
	try {
		auto session = std::make_unique<dune_session>();
		// Construct the game in non-interactive (AI) mode for v1. PR 4b will
		// add a constructor variant that injects an FFIAsyncAdapter so the
		// host can pause/resume.
		session->game = std::make_unique<Game>(num_players, seed, /*interactive=*/false);

		// Subscribe to the engine's event bus. The callback runs on the same
		// thread that called publish() (i.e., the run_to_end caller's thread
		// in v1). Lock-then-push is cheap, and decouples bus-emit cadence
		// from poll_event cadence.
		dune_session* raw = session.get();
		raw->subscription_id = raw->game->getEventBus().subscribe(
			[raw](const GameEvent& e) {
				std::lock_guard<std::mutex> lock(raw->queue_mutex);
				raw->event_queue.push_back(e);
			});

		return session.release();
	} catch (...) {
		return nullptr;
	}
}

void dune_session_destroy(dune_session_t* session) {
	if (!session) return;
	try {
		// Unsubscribe before tearing down the game so a late publish doesn't
		// reach a moved-from queue. Subscriptions outlive the bus only if
		// the bus is still alive — game owns the bus, so order matters.
		if (session->subscription_id != 0 && session->game) {
			session->game->getEventBus().unsubscribe(session->subscription_id);
		}
	} catch (...) {
		// best-effort teardown
	}
	delete session;
}

int dune_session_run_to_end(dune_session_t* session) {
	if (!session || !session->game) return DUNE_ERR_ARG;
	if (session->ran_to_end) return DUNE_DONE;
	try {
		session->game->runGame();
		session->ran_to_end = true;
		return DUNE_DONE;
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

int dune_session_get_snapshot(dune_session_t* session, char** out_json) {
	if (!session || !session->game || !out_json) return DUNE_ERR_ARG;
	try {
		std::string json = SnapshotBuilder::buildSnapshotJson(*session->game);
		*out_json = duplicateForCaller(json);
		return *out_json ? DUNE_OK : DUNE_ERR_INTERNAL;
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

int dune_session_poll_event(dune_session_t* session, char** out_json) {
	if (!session || !out_json) return DUNE_ERR_ARG;
	try {
		GameEvent next;
		{
			std::lock_guard<std::mutex> lock(session->queue_mutex);
			if (session->event_queue.empty()) return DUNE_NO_EVENT;
			next = std::move(session->event_queue.front());
			session->event_queue.pop_front();
		}
		std::string json = EventSerialization::toJson(next.event);
		*out_json = duplicateForCaller(json);
		return *out_json ? DUNE_OK : DUNE_ERR_INTERNAL;
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

void dune_free(char* p) {
	delete[] p;
}

} // extern "C"
