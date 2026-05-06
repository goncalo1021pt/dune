#include "ffi/ffi_async_adapter.hpp"

#include <utility>

namespace FFIAsync {

std::optional<DecisionResponse> FFIAsyncAdapter::requestDecision(const DecisionRequest& req) {
	std::unique_lock<std::mutex> lock(channel_->mu);

	// Pre-check cancel: destroy() may have been called between the previous
	// requestDecision/submit pair and this one (engine code ran between
	// decisions and never re-entered the wait).
	if (channel_->cancelRequested) throw SessionCancelled{};

	channel_->pendingRequest = req;
	channel_->state = SessionState::AwaitingDecision;
	channel_->cv.notify_all();  // wake host's step()/submit() waiting for transition

	channel_->cv.wait(lock, [&] {
		return channel_->state == SessionState::Running ||
		       channel_->cancelRequested;
	});

	if (channel_->cancelRequested) throw SessionCancelled{};

	// Consume the response. Reset the slot so a stale response can never
	// silently leak into the next decision if the protocol is mis-driven.
	DecisionResponse resp = std::move(channel_->pendingResponse);
	channel_->pendingResponse = DecisionResponse{};
	return resp;
}

} // namespace FFIAsync
