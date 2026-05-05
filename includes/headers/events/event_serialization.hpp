#pragma once

#include <string>

struct Event;

// Serialises a single Event into a JSON document the FFI layer can hand
// across the C ABI. Format is part of the v1 frozen schema and matches
// the snapshot's event payload shape.
namespace EventSerialization {

// Returns a stable string identifier for the EventType (e.g., "STORM_MOVED").
const char* eventTypeName(int eventTypeOrdinal);

// Returns a JSON document (single line, UTF-8) for one event.
std::string toJson(const Event& event);

} // namespace EventSerialization
