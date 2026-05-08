// recording_adapter.hpp — capture (request, response) pairs from a live
// IInteractionAdapter for later replay (Phase 5).
//
// Wraps an inner adapter (the live policy: TtyAdapter, FFIAsyncAdapter,
// or a deterministic test policy) and forwards every requestDecision
// call through, capturing both sides. The captured trace flows directly
// into a ScriptedAdapter to replay the same decision sequence.
//
// Lifetime: the wrapped adapter is held by reference; the caller must
// ensure it outlives the RecordingAdapter. Typical pattern in tests is
// stack-local policy + heap-allocated recorder owned by Game (see
// replay_test.cpp).

#pragma once

#include "interaction_adapter.hpp"

#include <vector>

struct DecisionTraceEntry {
	DecisionRequest request;
	DecisionResponse response;
};

class RecordingAdapter : public IInteractionAdapter {
public:
	explicit RecordingAdapter(IInteractionAdapter& wrapped) : wrapped_(wrapped) {}

	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override;

	const std::vector<DecisionTraceEntry>& trace() const { return trace_; }
	std::size_t entryCount() const { return trace_.size(); }

private:
	IInteractionAdapter& wrapped_;
	std::vector<DecisionTraceEntry> trace_;
};
