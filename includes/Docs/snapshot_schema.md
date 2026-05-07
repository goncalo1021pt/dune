# Snapshot schema (v1, PR 4a)

The Dune engine emits two kinds of JSON document at the FFI boundary:

1. A **snapshot** — the full game state at a point in time. Built on demand
   via `dune_session_get_snapshot()` and `SnapshotBuilder::buildSnapshotJson()`.
2. An **event** — a single game-event record. Drained from the per-session
   queue via `dune_session_poll_event()` and built by `EventSerialization::toJson()`.

Both shapes are part of the v1 frozen surface. Field renames or removals
are **breaking changes**; new fields can be added without a version bump
as long as existing readers can ignore them.

The schema is intentionally redaction-naive — every field is present in
every snapshot regardless of who's asking. The server (or Godot host) is
responsible for filtering hidden information per recipient before sending
to clients.

---

## Snapshot

```jsonc
{
  "schema_version": 1,

  "game": {
    "turn":         <int>,        // 0 before runGame; 1..MAX_TURNS during play
    "phase":        <string>,     // "STORM" | "SPICE_BLOW" | ... | "MENTAT_PAUSE"
    "turn_order":   [<int>, ...], // player indices in storm-derived order
    "player_count": <int>,        // 2..6
    "game_ended":   <bool>,
    "initial_seed": <uint>,       // value passed to dune_session_create
    "interactive":  <bool>        // false for FFI v1; true for the TTY CLI
  },

  "feature_settings": {
    "advanced_faction_abilities": <bool>,
    "increased_spice_flow":       <bool>,
    "advanced_combat":            <bool>,
    "advanced_double_spice_blow": <bool>
  },

  "storm": {
    "sector":    <int>,   // 0 before turn 1, otherwise 1..18
    "last_card": <int>,   // movement value used last turn
    "next_card": <int>    // value drawn for the upcoming move
  },

  "players": [
    {
      "faction_index":    <int>,           // 0..5
      "faction_name":     <string>,
      "home_sector":      <int>,
      "spice":            <int>,
      "reserve":          { "normal": <int>, "elite": <int> },
      "deployed":         { "normal": <int>, "elite": <int> },
      "destroyed":        { "normal": <int>, "elite": <int> },
      "free_revives":     <int>,
      "treachery_cards":  [<string>, ...], // by name; PR 4 follow-up may switch to IDs
      "traitor_cards":    [<string>, ...],
      "leaders_alive":    [{ "name": <string>, "power": <int>, "has_battled": <bool> }, ...],
      "leaders_dead":     [{ "name": <string>, "power": <int>, "has_battled": <bool> }, ...],

      // OPTIONAL — only present for the faction that owns this state.
      "captured_leaders": [{ "name": <string>, "power": <int>, ... }, ...], // Harkonnen only
      "prediction": {                                                       // Bene Gesserit only
        "set":     <bool>,
        "faction": <string>,
        "turn":    <int>
      }
    }
    // ... one entry per active player
  ],

  "map": {
    "territories": [
      {
        "name":             <string>,
        "terrain":          <string>,                  // "desert" | "rock" | "city" | "northPole"
        "sectors":          [<int>, ...],
        "has_transporter":  <bool>,                    // true for Arrakeen + Carthag
        "special_movement": <bool>,                    // true for Arrakeen + Carthag
        "units": [
          {
            "faction_index": <int>,
            "normal":        <int>,
            "elite":         <int>,
            "advisor":       <bool>,                   // BG peaceful state
            "sector":        <int>
          }
          // ... one entry per stack present
        ],
        "spice": [
          { "amount": <int>, "sector": <int> }
          // ... one entry per spice pile
        ]
      }
      // ... one entry per territory on the board
    ]
  },

  "treachery_discard": [<string>, ...],   // discard pile, public information

  "reactions": {
    "current_window": <string>,           // ReactionWindow name; "None" when nothing is open
    "window_open":    <bool>
  }
}
```

## Event

```jsonc
{
  "type":           <string>,   // EventType identifier, e.g. "STORM_MOVED"
  "message":        <string>,
  "turn":           <int>,
  "phase":          <string>,
  "player_faction": <string>,   // empty when the event isn't tied to a faction
  "territory":      <string>,
  "unit_count":     <int>,
  "spice_value":    <int>,
  "leader_power":   <int>,
  "visibility":     <string>,   // "public" | "private_to_actor"
  "actor_faction":  <int>       // -1 when not actor-private
}
```

The visibility tag is the seam the multiplayer server uses to redact events.
v1 sites all emit `visibility: "public"`; private events will appear in
later PRs as hidden-information sites are converted.

## Versioning

`schema_version: 1` is fixed for the v1 surface. The next bump (`2`)
will land alongside any breaking change. All v1 readers should accept
unknown extra fields silently — that's how additive changes ship.

---

## Decision protocol (v2, PR 4b)

Interactive sessions (`dune_session_create_interactive`) drive the
engine through a request/submit loop. The host calls `dune_session_step`
to advance until either a decision is needed or the game ends; on
`DUNE_PENDING` the host reads the request, decides, and submits a
response.

### Pending request (engine → host, via `dune_session_get_pending_decision`)

```jsonc
{
  "correlation_id": <uint>,
  "kind":           <string>,    // "yn" | "int" | "select"  (primitive only since PR 4c)
  "actor_index":    <int>,       // 0..5; -1 if not actor-specific
  "prompt":         <string>,
  "options":        [<string>, ...],   // populated for "select"
  "allow_none":     <bool>,            // for "select"; true means empty value is legal
  "int_min":        <int>,             // for "int" (inclusive)
  "int_max":        <int>              // for "int" (inclusive)
}
```

Only primitive kinds since PR 4c. The engine drives multi-step flows
(deployment, movement) by emitting *sequences* of these primitives, so a
host only ever has to render and answer one simple decision at a time.
A typical deployment is select-territory → int-units → int-elite-split →
select-sector; a typical movement adds a source-sector pick at the start.

### Submit response (host → engine, via `dune_session_submit_decision`)

```jsonc
{
  "value":           <string>,         // becomes DecisionResponse.payload_json verbatim
  "correlation_id":  <uint>            // optional; echoed back from the request if set
}
```

The `value` string is stuffed into `DecisionResponse.payload_json` without
any further interpretation by the FFI layer. The engine's existing
parsing consumes `payload_json` directly, so the same string shape that
the TtyAdapter writes on the CLI side flows through here unchanged.

### Per-kind value shapes

| `kind`   | `value` shape                                                                                                |
|----------|--------------------------------------------------------------------------------------------------------------|
| `yn`     | `"y"` or `"n"`                                                                                               |
| `int`    | decimal integer as string, e.g. `"5"`. Must satisfy `int_min ≤ value ≤ int_max`.                             |
| `select` | one of the strings in `options[]`, or `""` if `allow_none: true` (the engine treats empty value as a skip).  |

### Return-code matrix

| Function                            | Idle         | Running        | AwaitingDecision | Done       | Error             |
|-------------------------------------|--------------|----------------|------------------|------------|-------------------|
| `dune_session_step`                 | spawn worker; wait | wait    | `DUNE_ERR_STATE` | `DUNE_DONE`| `DUNE_ERR_INTERNAL` |
| `dune_session_get_pending_decision` | `DUNE_ERR_STATE` | `DUNE_ERR_STATE` | `DUNE_OK`    | `DUNE_ERR_STATE` | `DUNE_ERR_STATE` |
| `dune_session_submit_decision`      | `DUNE_ERR_STATE` | `DUNE_ERR_STATE` | wait → state | `DUNE_ERR_STATE` | `DUNE_ERR_STATE` |
| `dune_session_run_to_end`           | `DUNE_ERR_STATE` (interactive sessions reject) | — | — | — | — |

AI-mode sessions (`dune_session_create`) reject all v2 symbols with
`DUNE_ERR_STATE`. Interactive sessions reject `dune_session_run_to_end`
the same way.

### Cancellation

`dune_session_destroy` is always safe to call. On an interactive session
with the worker mid-decision: the FFI sets a cancel flag, notifies the
condvar, and joins. `FFIAsyncAdapter::requestDecision` wakes, sees the
flag, throws `SessionCancelled` inside the worker thread; the worker's
outer `try` catches it and exits. No detach, no leak.
