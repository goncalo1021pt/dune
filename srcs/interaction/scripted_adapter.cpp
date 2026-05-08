#include "interaction/scripted_adapter.hpp"

#include <utility>

ScriptedAdapter::ScriptedAdapter(std::vector<DecisionTraceEntry> entries)
	: entries_(std::move(entries)) {}

std::optional<DecisionResponse> ScriptedAdapter::requestDecision(const DecisionRequest& req) {
	if (next_ >= entries_.size()) {
		// Engine asked for more decisions than the trace has — replay is
		// out of sync. Return an invalid response so the engine treats it
		// as a refusal / skip rather than crashing.
		in_sync_ = false;
		DecisionResponse resp;
		resp.correlation_id = req.correlation_id;
		resp.valid = false;
		return resp;
	}

	const DecisionTraceEntry& entry = entries_[next_++];
	if (entry.request.kind != req.kind || entry.request.actor_index != req.actor_index) {
		// Kind / actor drift: the recording and the live run took different
		// decision paths. The scripted response is no longer meaningful.
		// Mark out-of-sync but still return SOMETHING so the engine doesn't
		// hang waiting for a response.
		in_sync_ = false;
	}
	return entry.response;
}
