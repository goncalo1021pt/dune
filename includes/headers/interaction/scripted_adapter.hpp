// scripted_adapter.hpp — replay-side IInteractionAdapter (Phase 5).
//
// Pairs with RecordingAdapter: the recorder captures (request, response)
// pairs from a live run; the scripted adapter replays them in order.
// On each requestDecision call the script verifies the live request's
// kind matches what was recorded, then returns the recorded response.
//
// A kind/actor mismatch means the engine took a different decision path
// in the replay than in the recording — i.e. determinism is broken or
// the trace was captured against a different (seed, settings) tuple.
// The mismatch is signalled by returning an invalid response (valid=false);
// tests assert on it via the trailing remaining()/consumed() counts.

#pragma once

#include "interaction_adapter.hpp"
#include "recording_adapter.hpp"

#include <vector>

class ScriptedAdapter : public IInteractionAdapter {
public:
	// Takes ownership of the trace; subsequent requestDecision calls walk
	// it in order. The trace usually comes from RecordingAdapter::trace().
	explicit ScriptedAdapter(std::vector<DecisionTraceEntry> entries);

	std::optional<DecisionResponse> requestDecision(const DecisionRequest& req) override;

	// Number of trace entries consumed so far (== number of requestDecision
	// calls served, regardless of whether the kind matched).
	std::size_t consumed() const { return next_; }
	std::size_t remaining() const { return entries_.size() - next_; }

	// True iff every requestDecision so far observed the recorded kind /
	// actor. Set false the first time a mismatch occurs.
	bool inSync() const { return in_sync_; }

private:
	std::vector<DecisionTraceEntry> entries_;
	std::size_t next_ = 0;
	bool in_sync_ = true;
};
