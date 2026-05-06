// decision_serialization.hpp — JSON shapes for the FFI v2 decision protocol
// (PR 4b). The host process never sees DecisionRequest / DecisionResponse
// directly; they cross the C ABI boundary as JSON strings produced and
// consumed by the helpers in this header.
//
// Schema is intentionally minimal:
//   - requestToJson  drops the void* migration_ctx (a raw PhaseContext pointer
//                    that's a dangling reference from the host's perspective
//                    and a security smell to expose). PR 4c removes the field
//                    from DecisionRequest itself.
//   - parseSubmitJson accepts {"value": "<string>"} and stuffs the value
//                    string into DecisionResponse.payload_json verbatim. The
//                    engine's existing parsing code consumes payload_json
//                    unchanged, so simple kinds (yn/int/select) and compound
//                    kinds (deployment/movement) flow through the same path.

#pragma once

#include <optional>
#include <string>

struct DecisionRequest;
struct DecisionResponse;

namespace DecisionSerialization {

// Serialize a DecisionRequest to the JSON shape the host will receive via
// dune_session_get_pending_decision. Never includes migration_ctx.
std::string requestToJson(const DecisionRequest& req);

// Parse a host-submitted JSON string. Returns std::nullopt on malformed
// input (missing "value" field, not an object, parse error). The returned
// response has payload_json set verbatim from the input's "value"; valid is
// always true on a successful parse — it's the engine's job to validate the
// payload semantically.
std::optional<DecisionResponse> parseSubmitJson(const std::string& json);

} // namespace DecisionSerialization
