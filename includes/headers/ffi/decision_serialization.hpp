// decision_serialization.hpp — JSON shapes for the FFI v2 decision protocol
// (PR 4b/4c). The host process never sees DecisionRequest / DecisionResponse
// directly; they cross the C ABI boundary as JSON strings produced and
// consumed by the helpers in this header.
//
// Schema is intentionally minimal — only primitive kinds (yn/int/select)
// since PR 4c. The engine drives multi-step flows (deployment, movement)
// by issuing sequences of these primitives, so the host only ever has to
// answer one simple decision at a time.
//
//   - requestToJson  serialises a DecisionRequest as a flat JSON object.
//   - parseSubmitJson accepts {"value": "<string>"} and stuffs the value
//                    string into DecisionResponse.payload_json verbatim. The
//                    engine's existing parsing code consumes payload_json
//                    unchanged.

#pragma once

#include <optional>
#include <string>

struct DecisionRequest;
struct DecisionResponse;

namespace DecisionSerialization {

// Serialize a DecisionRequest to the JSON shape the host will receive via
// dune_session_get_pending_decision. Output contains only the documented
// primitive-kind fields (correlation_id, kind, actor_index, prompt,
// options, allow_none, int_min, int_max).
std::string requestToJson(const DecisionRequest& req);

// Parse a host-submitted JSON string. Returns std::nullopt on malformed
// input (missing "value" field, not an object, parse error). The returned
// response has payload_json set verbatim from the input's "value"; valid is
// always true on a successful parse — it's the engine's job to validate the
// payload semantically.
std::optional<DecisionResponse> parseSubmitJson(const std::string& json);

} // namespace DecisionSerialization
