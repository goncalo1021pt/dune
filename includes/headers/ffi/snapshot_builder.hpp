#pragma once

#include <string>

class Game;

// SnapshotBuilder serialises the current Game state into a stable JSON shape
// that the Godot frontend (and future server/clients) can consume.
//
// Schema is documented in includes/Docs/snapshot_schema.md and frozen as
// part of FFI v1 — field renames are breaking changes.
//
// Hidden-information redaction is NOT performed here: this returns the full
// game state. The server-side filter (a future component) is responsible
// for stripping per-player hidden info before forwarding to clients.
namespace SnapshotBuilder {

// Returns a JSON document (single line, UTF-8) describing every observable
// field of the game state. The function is read-only and thread-safe with
// respect to a paused game (caller is responsible for not mutating the
// game concurrently).
std::string buildSnapshotJson(const Game& game);

} // namespace SnapshotBuilder
