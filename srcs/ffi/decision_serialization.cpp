#include "ffi/decision_serialization.hpp"
#include "interaction/interaction_adapter.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace DecisionSerialization {

std::string requestToJson(const DecisionRequest& req) {
	json out = {
		{"correlation_id", req.correlation_id},
		{"kind",           req.kind},
		{"actor_index",    req.actor_index},
		{"prompt",         req.prompt},
		{"options",        req.options},
		{"allow_none",     req.allow_none},
		{"int_min",        req.int_min},
		{"int_max",        req.int_max},
	};
	// Note: migration_ctx is deliberately omitted — it's a raw PhaseContext*
	// that's meaningless and unsafe across the C ABI. PR 4c removes the
	// field from DecisionRequest itself.
	return out.dump();
}

std::optional<DecisionResponse> parseSubmitJson(const std::string& jsonStr) {
	json parsed;
	try {
		parsed = json::parse(jsonStr);
	} catch (const json::exception&) {
		return std::nullopt;
	}
	if (!parsed.is_object()) return std::nullopt;
	auto it = parsed.find("value");
	if (it == parsed.end() || !it->is_string()) return std::nullopt;

	DecisionResponse resp;
	resp.payload_json = it->get<std::string>();
	resp.valid = true;
	if (auto cit = parsed.find("correlation_id"); cit != parsed.end() && cit->is_number_unsigned()) {
		resp.correlation_id = cit->get<uint64_t>();
	}
	return resp;
}

} // namespace DecisionSerialization
