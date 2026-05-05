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
