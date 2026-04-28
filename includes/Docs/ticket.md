# Ticket: Cohesive Event System + Godot Integration Restructure

## Title
Core engine restructure for cohesive event bus, hook-driven gameplay flow, and shared-library integration for Godot clients.

## Context
The current engine is phase-driven and works in TTY mode, but card reactions and future multiplayer clients need a central, deterministic event model.

We need to:
- Move from scattered ad-hoc prompts/callbacks to a cohesive event bus.
- Prepare a clean C ABI boundary for Godot integration.
- Compile the engine as a shared library (`.so`) usable by external clients.
- Ensure game state and event stream are reliable, serializable, and client-agnostic.

## Goals
1. Introduce a deterministic `GameEventBus` and reaction pipeline for gameplay hooks.
2. Normalize all phase and card triggers as typed events.
3. Split core rules from presentation (TTY/Godot adapters).
4. Add a stable C-compatible API wrapper around core engine features.
5. Support both sync TTY decisions and async UI/network decisions.
6. Keep existing behavior working while migrating incrementally.

## Non-Goals
- Full card implementation in this ticket.
- Full Godot UI implementation.
- Multiplayer authority model implementation (only event/state plumbing readiness).

## High-Level Architecture

### 1) Engine Core (pure C++)
- Owns all game rules, state mutation, and deterministic RNG.
- No direct `std::cin/std::cout` usage in rule code.
- Emits typed events only.

### 2) Event Layer
- `GameEventBus`: in-process publish/subscribe for engine systems.
- `ReactionEngine`: consumes pre-defined reaction windows and validates legal responses.
- `EventRecorder`: persists ordered event log for replay/debug.

### 3) Interaction Adapter Layer
- `IInteractionAdapter` interface (TTY adapter now, Godot adapter later).
- Engine requests decisions through adapter rather than direct console reads.
- Adapter returns structured decisions with correlation IDs.

### 4) ABI Wrapper Layer
- `libdune.so` exposes C API:
  - create/destroy game session
  - step engine
  - poll events
  - submit decisions
  - query snapshots
- Godot binds to C API (GDExtension/Native integration).

## Proposed Module Layout

### New headers (suggested)
- `includes/headers/events/game_event.hpp`
- `includes/headers/events/game_event_bus.hpp`
- `includes/headers/events/event_types.hpp`
- `includes/headers/reactions/reaction_engine.hpp`
- `includes/headers/reactions/reaction_window.hpp`
- `includes/headers/interaction/interaction_adapter.hpp`
- `includes/headers/ffi/dune_c_api.h`

### New sources (suggested)
- `srcs/events/game_event_bus.cpp`
- `srcs/events/event_serialization.cpp`
- `srcs/reactions/reaction_engine.cpp`
- `srcs/interaction/tty_adapter.cpp`
- `srcs/ffi/dune_c_api.cpp`

## Event Model

### Event categories
- Lifecycle: game started, turn started, phase entered/exited.
- State changes: units moved/shipped, spice changed, cards drawn/played/discarded.
- Decision requests: player must choose action.
- Reactions: reaction window opened/closed, reaction accepted/rejected.
- Errors/warnings: invalid decision, rule conflict.

### Required event fields
- `event_id` (monotonic uint64)
- `turn`
- `phase`
- `timestamp` (engine monotonic tick, not wall-clock)
- `actor_faction`
- `type`
- `payload` (typed structure; JSON export optional)
- `correlation_id` (for request/response decisions)

### Determinism requirements
- Event order must be deterministic for same seed + same decisions.
- No client-side randomness in rule resolution.
- All random draws are engine-originated and emitted as events.

## Hook/Reaction Windows (initial matrix)

### Global
- `AnytimeWindow` (opt-in at safe checkpoints)

### Storm
- `BeforeStormMove` (Weather Control)
- `AfterStormMove`

### Spice Blow / Nexus equivalent checkpoints
- `AfterSpiceBlow`
- `BeforeShipmentPhase` (Bene advisory battle declaration already close to this)

### Shipment/Move
- `BeforeShipment(player)`
- `AfterShipment(player)`
- `BeforeMovement(player)`
- `AfterMovement(player)`

### Battle
- `BeforeBattlePlanReveal`
- `AfterBattlePlanReveal`
- `BeforeBattleResolution`
- `AfterBattleResolution`

### Revival
- `BeforeRevival(player)`
- `AfterRevival(player)`

## Card Integration Strategy

### Short-term cards
- Lasgun: currently phase-local; move trigger and effect reporting into bus events.
- Hajr: emit `ExtraMoveGranted` and consume through reaction window.
- Weather Control: resolve via `BeforeStormMove` reaction.
- Tleilaxu Ghola: implement as true reaction in `AnytimeWindow` + `BeforeRevival/AfterBattle` checkpoints.

### Validation rules
- Each card defines:
  - legal windows
  - preconditions
  - required decision payload
  - side effects
  - discard timing

## Game State Cohesion Plan

### Current pain points
- State mutations spread across phases and helper functions.
- Debug/event logs and state are not strictly unified.

### Target
- All state mutations happen through command-like engine actions that emit events.
- Snapshot generation is centralized and cheap to query.
- Event log can reconstruct state for replay and desync checks.

### Snapshot contract
Provide stable snapshot structs for external clients:
- game summary (turn, phase, storm, turn order)
- players (resources, reserves, destroyed, leaders, cards count)
- map (territories, stacks, spice, sectors)
- pending decisions/reaction windows

## Shared Library (`.so`) Plan

### Build outputs
- Keep CLI binary (`dune`) for local testing.
- Add `libdune.so` target for embedding.

### C API surface (minimal v1)
- `dune_session_create(config_json)`
- `dune_session_destroy(handle)`
- `dune_session_step(handle)`
- `dune_session_get_snapshot(handle, out_json)`
- `dune_session_poll_event(handle, out_event_json)`
- `dune_session_submit_decision(handle, decision_json)`

### ABI safety rules
- C ABI only in exported boundary.
- No STL types across ABI.
- Versioned structs/functions.
- Ownership rules explicit for buffers.

## Migration Plan (phased)

### Phase 0: Scaffolding
- Add event bus + event type definitions.
- Add interaction adapter interface.
- Wrap existing logger emission into event dispatch bridge.

### Phase 1: Adapter extraction
- Remove direct phase input reads progressively.
- Route all prompts through `IInteractionAdapter`.
- Keep TTY adapter as default.

### Phase 2: Reaction engine
- Add reaction windows and validation.
- Migrate Weather Control + Hajr to windows.
- Add Tleilaxu Ghola as true anytime reaction.

### Phase 3: State/snapshot cohesion
- Centralize mutation events and snapshot builder.
- Add event recorder and replay harness.

### Phase 4: FFI + shared lib
- Add C API wrapper and `libdune.so` build target.
- Add smoke test app that links `.so`.

### Phase 5: Hardening
- Determinism tests across seeds.
- Compatibility checks for TTY and library modes.

## Acceptance Criteria
1. Engine runs with no direct terminal I/O in core rules path.
2. Decision requests are emitted as events and resolved by adapter.
3. Weather Control/Hajr/Tleilaxu Ghola execute through reaction windows.
4. Snapshot API exposes complete game state needed by client UI.
5. `libdune.so` builds and a small integration harness can:
   - create session
   - step game
   - read events
   - submit decisions
6. Deterministic replay passes for fixed seed + decision trace.

## Testing Strategy
- Unit tests:
  - event bus ordering
  - reaction legality checks
  - serialization round-trip
- Integration tests:
  - scripted turn progression from events/decisions
  - card reaction scenarios
- Regression tests:
  - compare key outcomes with current behavior
- FFI tests:
  - C API lifetime and memory safety

## Risks
1. Large cross-phase refactor can introduce behavior regressions.
2. Mixing old direct calls and new adapter flow can cause duplicated decisions.
3. ABI mistakes can break Godot integration stability.
4. Event payload bloat can impact performance if not controlled.

## Mitigations
- Introduce feature flags per migrated subsystem.
- Keep dual path temporarily with strict assertions to avoid duplicate execution.
- Freeze C API v1 minimal surface before expansion.
- Provide compact binary event option later if JSON overhead is high.

## Open Questions
1. Should we formalize a Nexus phase now or keep checkpoint hooks in existing phases?
2. Do we want one global reaction priority policy (turn order, seat order, timestamp)?
3. For Godot multiplayer, will server be authoritative over event stream from day one?
4. Should hidden information be redacted at source (engine) or per-client adapter?

## Deliverables
- `ticket.md` (this plan)
- New event/reaction/adapter modules
- Updated Makefile with `.so` target
- Initial C API wrapper
- Migration PRs by phase with compatibility notes
