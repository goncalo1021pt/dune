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
#include "ffi/decision_serialization.hpp"
#include "ffi/ffi_async_adapter.hpp"
#include "events/event_serialization.hpp"
#include "events/game_event_bus.hpp"
#include "game.hpp"

#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <thread>

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
//
// v2 fields (interactive sessions only): channel + worker thread. AI-mode
// sessions leave them at default-constructed values and never spawn a worker.
struct dune_session {
	std::unique_ptr<Game> game;
	std::deque<GameEvent> event_queue;
	std::mutex queue_mutex;  // protects event_queue against the bus callback
	GameEventBus::SubscriptionId subscription_id = 0;
	bool ran_to_end = false;

	// v2: interactive plumbing.
	bool interactive = false;
	FFIAsync::DecisionChannel channel;  // shared sync state
	std::thread worker;
};

namespace {

// Worker-thread entry point. Runs Game::runGame; catches SessionCancelled
// (host called destroy mid-decision) and any other exception so the thread
// always exits cleanly. The final state transition (Done/Error) wakes any
// host thread parked in step()/submit_decision().
void worker_main(dune_session* s) {
	try {
		s->game->runGame();
	} catch (const FFIAsync::SessionCancelled&) {
		// Cooperative cancel — expected on destroy.
		std::lock_guard<std::mutex> lock(s->channel.mu);
		s->channel.state = FFIAsync::SessionState::Done;
		s->channel.cv.notify_all();
		return;
	} catch (...) {
		std::lock_guard<std::mutex> lock(s->channel.mu);
		s->channel.state = FFIAsync::SessionState::Error;
		s->channel.cv.notify_all();
		return;
	}
	std::lock_guard<std::mutex> lock(s->channel.mu);
	s->channel.state = FFIAsync::SessionState::Done;
	s->channel.cv.notify_all();
}

// Translate the channel's terminal/stable state to a v2 ABI return code.
// MUST be called with channel.mu held.
int stableStateToReturnCode(FFIAsync::SessionState state) {
	switch (state) {
		case FFIAsync::SessionState::AwaitingDecision: return DUNE_PENDING;
		case FFIAsync::SessionState::Done:             return DUNE_DONE;
		case FFIAsync::SessionState::Error:            return DUNE_ERR_INTERNAL;
		default:                                       return DUNE_ERR_INTERNAL;
	}
}

} // namespace

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

	// Interactive sessions: cooperatively signal the worker and join.
	// If the worker is parked inside FFIAsyncAdapter::requestDecision, the
	// cancel flag wakes it and the adapter throws SessionCancelled, which
	// worker_main catches. If the engine is between two decisions, the
	// flag is observed at the next requestDecision.
	if (session->worker.joinable()) {
		{
			std::lock_guard<std::mutex> lock(session->channel.mu);
			session->channel.cancelRequested = true;
		}
		session->channel.cv.notify_all();
		try {
			session->worker.join();
		} catch (...) {
			// best-effort: leave the thread dangling rather than throwing
			// from destroy. Should be impossible in practice.
		}
	}

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
	// run_to_end is the v1 AI-mode entry point. Interactive sessions must
	// drive via step()/submit_decision() — calling run_to_end on them
	// would hang the host thread inside the adapter.
	if (session->interactive) return DUNE_ERR_STATE;
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

/* ---- v2: interactive session (PR 4b) ------------------------------ */

dune_session_t* dune_session_create_interactive(unsigned int seed, int num_players) {
	if (num_players < 2 || num_players > 6) return nullptr;
	try {
		auto session = std::make_unique<dune_session>();
		session->interactive = true;

		// Construct the game in interactive mode. The constructor wires up
		// a TtyAdapter; we immediately replace it with FFIAsyncAdapter that
		// shares the session's DecisionChannel. Replacement must happen
		// before initializeGame() / runGame() is called so starting-force
		// placement (Fremen, etc.) sees the right adapter.
		session->game = std::make_unique<Game>(num_players, seed, /*interactive=*/true);
		session->game->setInteractionAdapter(
			std::make_unique<FFIAsync::FFIAsyncAdapter>(&session->channel));

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

int dune_session_step(dune_session_t* session) {
	if (!session || !session->game) return DUNE_ERR_ARG;
	if (!session->interactive) return DUNE_ERR_STATE;

	try {
		std::unique_lock<std::mutex> lock(session->channel.mu);

		// Refuse step() while a decision is pending — host must submit first.
		if (session->channel.state == FFIAsync::SessionState::AwaitingDecision) {
			return DUNE_ERR_STATE;
		}
		if (session->channel.state == FFIAsync::SessionState::Done) {
			return DUNE_DONE;
		}
		if (session->channel.state == FFIAsync::SessionState::Error) {
			return DUNE_ERR_INTERNAL;
		}

		// Idle: spawn the worker on first step(). Transition to Running so
		// the worker's first state observation is consistent.
		if (session->channel.state == FFIAsync::SessionState::Idle) {
			session->channel.state = FFIAsync::SessionState::Running;
			lock.unlock();
			session->worker = std::thread(worker_main, session);
			lock.lock();
		}

		// Wait for transition out of Running.
		session->channel.cv.wait(lock, [&] {
			return session->channel.state == FFIAsync::SessionState::AwaitingDecision ||
			       session->channel.state == FFIAsync::SessionState::Done ||
			       session->channel.state == FFIAsync::SessionState::Error;
		});

		return stableStateToReturnCode(session->channel.state);
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

int dune_session_get_pending_decision(dune_session_t* session, char** out_json) {
	if (!session || !session->game || !out_json) return DUNE_ERR_ARG;
	if (!session->interactive) return DUNE_ERR_STATE;

	try {
		std::lock_guard<std::mutex> lock(session->channel.mu);
		if (session->channel.state != FFIAsync::SessionState::AwaitingDecision) {
			return DUNE_ERR_STATE;
		}
		std::string json = DecisionSerialization::requestToJson(session->channel.pendingRequest);
		*out_json = duplicateForCaller(json);
		return *out_json ? DUNE_OK : DUNE_ERR_INTERNAL;
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

int dune_session_submit_decision(dune_session_t* session, const char* in_json) {
	if (!session || !session->game || !in_json) return DUNE_ERR_ARG;
	if (!session->interactive) return DUNE_ERR_STATE;

	try {
		// Parse outside the lock — failure should not transition state.
		auto resp = DecisionSerialization::parseSubmitJson(in_json);
		if (!resp.has_value()) return DUNE_ERR_ARG;

		std::unique_lock<std::mutex> lock(session->channel.mu);
		if (session->channel.state != FFIAsync::SessionState::AwaitingDecision) {
			return DUNE_ERR_STATE;
		}

		// Hand the response to the worker and unblock it. The worker's
		// FFIAsyncAdapter::requestDecision will move-from this slot.
		session->channel.pendingResponse = std::move(resp.value());
		session->channel.state = FFIAsync::SessionState::Running;
		session->channel.cv.notify_all();

		// Wait for the next stable state (next decision, completion, or error).
		session->channel.cv.wait(lock, [&] {
			return session->channel.state == FFIAsync::SessionState::AwaitingDecision ||
			       session->channel.state == FFIAsync::SessionState::Done ||
			       session->channel.state == FFIAsync::SessionState::Error;
		});

		return stableStateToReturnCode(session->channel.state);
	} catch (...) {
		return DUNE_ERR_INTERNAL;
	}
}

} // extern "C"
