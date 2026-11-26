# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

C++17 implementation of the Dune (1979 / 2019 reprint) board game engine, plus a Godot 4 GDExtension frontend (in this same repo, currently being built). The CLI binary `dune` plays AI-vs-AI on stdout. `libdune.so` is what the Godot side links against. The ticket at [includes/Docs/ticket.md](includes/Docs/ticket.md) is the authoritative engine architecture document — its acceptance criteria are all green as of PR #34.

Monorepo layout: engine sources at the root (`srcs/`, `includes/`, `tests/`, `Makefile`); the Godot project will live alongside under a sibling directory (e.g. `godot/`) once it's started. Atomic commits across the engine ↔ frontend boundary are the main reason for the monorepo — during early integration you'll find snapshot fields or decision kinds the FFI is missing, and fixing both sides in one commit is the right call.

## Build commands

```sh
make              # CLI binary `dune` (AI-vs-AI on stdout)
make shared       # libdune.so for FFI/Godot consumption
make tests        # builds + runs the doctest binary at tests/run
make ffi_smoke    # pure-C smoke via the v1 FFI surface (run-to-end)
make ffi_smoke_interactive   # pure-C smoke via the v2 FFI surface (step / submit)
make re           # fclean + all
```

**`make re` is mandatory after any header layout change.** Adding/removing fields on `Event`, `DecisionRequest`, etc. produces ABI-incompatible object files in `objs/` that incremental `make` happily links into a memory-corrupting binary. We've hit this twice (PR #27, PR #30); both times it manifested as mid-game `munmap_chunk: invalid pointer` aborts. If a smoke test segfaults right after a struct edit, `make fclean && make` first — don't debug.

Compiler flags are unconditional: `-Wall -Wextra -Werror -g3 -std=c++17 -fPIC`. `-Werror` includes `-Werror=unused-function`, so a stale internal helper after a refactor breaks the build (this is a feature, not a bug).

### Running a single test

The test runner is one doctest binary. Filter by suite name:

```sh
make tests                                          # all
./tests/run --test-suite="Determinism"              # one suite
./tests/run --test-case="*replay*"                  # case glob
./tests/run --list-test-cases                       # discover
```

Test files (`tests/*.cpp`) are wildcard-globbed by the Makefile — drop a new `*_test.cpp` in `tests/` and it's auto-included on next `make tests`.

### CLI flags

`./dune` runs AI-vs-AI on a fixed seed. `./dune -d` enables `GameDebugger` instrumentation (signal-handler-based; CLI-only since the singleton would break libdune.so).

## Architecture

### The two-target split

`Makefile` separates sources into three buckets:

- **CLI-only** (`main.cpp`, `sighandler.cpp`, `debug.cpp`) — process-global state. Ship in `dune`, NEVER in `libdune.so`. The `GameDebugger` singleton assumes one game per process.
- **FFI-only** (`srcs/ffi/*.cpp`) — `extern "C"` symbols. Ship in `libdune.so`, NEVER in `dune` (would pollute the executable's symbol table).
- **Core** — everything else, shared between both binaries and the test runner.

When adding a new source file, this split is implicit via path; just put it in the right directory.

### Engine layering (top down)

1. **`Game`** ([srcs/game.cpp](srcs/game.cpp), [includes/headers/game.hpp](includes/headers/game.hpp)) owns: players, map, decks, RNG (seeded `std::mt19937`), `GameEventBus`, `ReactionEngine`, an optional `IInteractionAdapter`. `runGame()` runs initialization + 10 turns. `setInteractionAdapter()` lets FFI swap in `FFIAsyncAdapter` post-construction.

2. **Phases** (`srcs/phases/*.cpp`) — strategy-pattern `Phase` subclasses, one per game phase (Storm, Spice Blow, CHOAM Charity, Bidding, Revival, Ship-and-Move, Battle, Spice Collection, Mentat Pause). Each `Phase::execute(PhaseContext&)` is the rule body. `PhaseContext` is the per-phase dependency-injection bag (players, map, decks, adapter, reactions, RNG, logger, feature settings).

3. **`ReactionEngine`** ([srcs/reactions/reaction_engine.cpp](srcs/reactions/reaction_engine.cpp)) brackets phase code with `ReactionWindow`s (`BeforeStormMove`, `BeforeFactionAdvantage`, `AnytimeSafe`, etc.). All reactive card plays (Weather Control, Hajr, Tleilaxu Ghola, Karama in all its forms) go through one of its `dispatchXxx` methods. `dispatchAnytimeSafe` is large and has known structural duplication across Ghola / Emperor-Karama / Fremen-Karama / Harkonnen-Karama loops — flagged as tech debt; refactor would consolidate into a table-driven dispatcher.

4. **`GameEventBus`** ([srcs/events/game_event_bus.cpp](srcs/events/game_event_bus.cpp)) — synchronous pub/sub. Every state change emits an event. Subscribers are notified in registration order via a parallel `vector<SubscriptionId>` so `unordered_map` iteration order doesn't leak into the event stream (this matters for determinism). `BusBridgeLogger` is the legacy console output; `EventRecorder` is the replay capture; `dune_session`'s FFI subscription drains into the host's poll queue.

5. **`IInteractionAdapter`** ([includes/headers/interaction/interaction_adapter.hpp](includes/headers/interaction/interaction_adapter.hpp)) is the engine's only way to ask a player a question. **The kinds are primitive only**: `yn`, `int`, `select`. Compound shapes (`deployment`, `movement`) were removed in PR #33 — the engine drives multi-step flows by emitting *sequences* of primitives. There are four implementations:
   - `TtyAdapter` — stdin/stdout, used by the CLI in `-d` interactive mode.
   - `FFIAsyncAdapter` — blocks on a condvar inside a worker thread; the FFI host drives via `dune_session_step`/`submit`.
   - `RecordingAdapter` / `ScriptedAdapter` (test-only) — capture / replay decision traces.

6. **FFI** (`srcs/ffi/*.cpp`) — frozen C ABI in [dune_c_api.h](includes/headers/ffi/dune_c_api.h). 12 `extern "C"` symbols: 8 v1 (synchronous run + snapshot/event polling) + 4 v2 (interactive step / get_pending_decision / submit_decision / create_interactive). All function bodies are wrapped in try/catch — exceptions never cross the boundary. String returns use `new char[]` and must be `dune_free`d by the host. Schema documented in [includes/Docs/snapshot_schema.md](includes/Docs/snapshot_schema.md); that file is what the Godot side reads, treat it as a contract.

### Determinism (post PR #34)

Same `(seed, decision trace)` → byte-identical event stream. Tested in [tests/determinism_test.cpp](tests/determinism_test.cpp) and [tests/replay_test.cpp](tests/replay_test.cpp). Don't introduce non-determinism — specifically:

- **No `rand()`** — use `std::uniform_int_distribution` over `ctx.rng` (the seeded `std::mt19937` on `Game`). PR #34 commit 0 fixed a `rand()` call in `battle_phase.cpp` that broke determinism the moment two `Game` instances coexisted in one process.
- **No `std::random_device`, no `chrono::now()`, no `getpid()`** in the engine path.
- **No reliance on `unordered_map` iteration order** crossing into observable behavior. If you need stable order, mirror what `GameEventBus` does (parallel `vector` of keys).

### Faction abilities

Each of the 6 factions has its own `FactionAbility` subclass (`srcs/factions/`). Ability hooks fire from phase code (e.g. `getShipmentCost`, `onBattleWon`, `onWormHitsTerritory`, `onOtherFactionPaidForCard`). Discrete-trigger abilities are wrapped by `ReactionEngine::dispatchKaramaBlock` so opponents can spend Karama to cancel them. **Faction comparison in the codebase uses `getFactionName() == "Atreides"` strings** in ~22 places — flagged as tech debt; a `faction` enum would be safer but the refactor isn't done yet.

## Conventions worth knowing

- **Tabs for indentation** (the codebase is consistent on this).
- **Doctest** for unit tests; `ffi_smoke*.c` are pure-C linker tests for the ABI.
- **Commit style**: PR-tracked phases (`PR Phase5 commit 0`, `PR 4c commit 1`, etc.), with a `Refs: #N` line linking to the planning issue. Each commit must build clean and pass tests at its boundary — no mid-refactor commits.
- **`./dune` seed-42 is the smoke baseline.** Many PR descriptions reference its outcome; a behavior change there isn't a regression by default but should be called out in the commit message.
- The Godot side lives **in this repo** under its own sibling directory. The C ABI (`dune_c_api.h`) and snapshot/event/decision JSON schema (`snapshot_schema.md`) are the contract between the two sides — treat them as frozen for cross-cutting changes, but a monorepo means you can update both atomically when a real gap surfaces.

## Documentation pointers

- [includes/Docs/ticket.md](includes/Docs/ticket.md) — original architecture ticket. All 6 acceptance criteria green as of PR #34.
- [includes/Docs/snapshot_schema.md](includes/Docs/snapshot_schema.md) — frozen JSON schema for snapshots, events, and decision protocol. The contract Godot reads.
- [includes/Docs/Leaders.csv](includes/Docs/Leaders.csv) and [includes/Docs/treachery_cards.csv](includes/Docs/treachery_cards.csv) — game data references.
- [includes/Docs/TICKET_faction_system.md](includes/Docs/TICKET_faction_system.md) — faction system notes.
