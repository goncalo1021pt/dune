// ffi_async_adapter.hpp — IInteractionAdapter implementation that bridges
// the engine's sync requestDecision contract to an async host (Godot,
// future server) via a worker thread + condvar (PR 4b).
//
// Lifecycle:
//   1. dune_session_create constructs a DecisionChannel inside the session
//      struct and an FFIAsyncAdapter pointing at it.
//   2. dune_session_step launches a worker thread running Game::runGame
//      with the FFIAsyncAdapter installed as the engine's adapter.
//   3. Engine code reaches a decision site, calls adapter->requestDecision.
//      The adapter sets channel->state = AwaitingDecision, notifies, and
//      blocks on the condvar.
//   4. Host's step() unblocks (state changed), returns DUNE_PENDING.
//   5. Host calls submit_decision; FFI layer writes channel->pendingResponse
//      and channel->state = Running, notifies.
//   6. Worker thread (still inside requestDecision) unblocks, returns the
//      response to the engine, which keeps running until the next decision
//      or until runGame returns.
//
// Cancel:
//   dune_session_destroy sets channel->cancelRequested = true and notifies.
//   If the worker is blocked inside requestDecision, it wakes, sees the
//   flag, throws SessionCancelled. The worker function's outer try/catch
//   swallows the exception, marks state = Done (or Cancelled), exits.
//   destroy() then joins. No detach, no leaks.

#pragma once

#include "interaction/interaction_adapter.hpp"

#include <condition_variable>
#include <exception>
#include <mutex>

namespace FFIAsync {

enum class SessionState {
	Idle,             // worker thread not yet launched
	Running,          // worker advancing the engine
	AwaitingDecision, // worker blocked in requestDecision; host should submit
	Done,             // game ended cleanly; worker exited
	Error,            // worker exited via unexpected exception
};

// Synchronization channel between the engine's worker thread and the host
// thread that calls the FFI surface. Lives inside the dune_session struct;
// FFIAsyncAdapter holds a non-owning pointer.
//
// Locking discipline: every read/write of any field below MUST hold `mu`.
// Both threads notify cv after every state transition.
struct DecisionChannel {
	std::mutex mu;
	std::condition_variable cv;
	SessionState state = SessionState::Idle;

	// Set by the worker (inside requestDecision) before transitioning to
	// AwaitingDecision. Read by the host via dune_session_get_pending_decision.
	DecisionRequest pendingRequest;

	// Set by the host (via dune_session_submit_decision) before transitioning
	// state back to Running. Consumed (move-from) by the worker on resume.
	DecisionResponse pendingResponse;

	// Set by dune_session_destroy. Observed at every cv.wait that the
	// adapter does. When true, requestDecision throws SessionCancelled.
	bool cancelRequested = false;
};

// Thrown out of FFIAsyncAdapter::requestDecision when the host has requested
// session destruction. Caught by the worker function so the thread exits
// cleanly. Engine code should not catch this.
struct SessionCancelled : public std::exception {
	const char* what() const noexcept override { return "FFI session cancelled"; }
};

class FFIAsyncAdapter : public IInteractionAdapter {
public:
	explicit FFIAsyncAdapter(DecisionChannel* channel) : channel_(channel) {}

	// Always returns a populated DecisionResponse synchronously from the
	// engine's perspective (blocks the worker thread until the host submits).
	// Throws SessionCancelled if the host requested cancellation before or
	// during the wait.
	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override;

private:
	DecisionChannel* channel_;
};

} // namespace FFIAsync
