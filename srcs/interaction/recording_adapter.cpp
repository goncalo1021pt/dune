#include "interaction/recording_adapter.hpp"

std::optional<DecisionResponse> RecordingAdapter::requestDecision(const DecisionRequest& req) {
	auto resp = wrapped_.requestDecision(req);
	DecisionTraceEntry entry;
	entry.request = req;
	if (resp.has_value()) {
		entry.response = *resp;
	} else {
		// Async wrapped adapters can return nullopt to signal "yield". The
		// recorder doesn't model that — it's only used with synchronous
		// policies in tests. Capture an invalid sentinel so a replay over
		// this trace fails loudly rather than silently.
		entry.response.valid = false;
	}
	trace_.push_back(entry);
	return resp;
}
