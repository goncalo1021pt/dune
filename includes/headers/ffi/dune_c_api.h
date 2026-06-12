/* dune_c_api.h — C ABI for libdune.so.
 *
 * This is the frozen surface that Godot/GDExtension links against. Everything
 * exported here must keep its signature stable — additions are fine, but
 * existing function signatures must not change without a major version bump.
 *
 * v1 (PR 4a): synchronous run + read-only snapshot/event polling.
 *   Sufficient to drive headless replay/visualisation in Godot.
 *
 * v2 (PR 4b, this file): non-blocking step + decision submit. Adds:
 *     - dune_session_create_interactive
 *     - dune_session_step
 *     - dune_session_get_pending_decision
 *     - dune_session_submit_decision
 *   Same opaque session handle, additive only. v1 callers keep working.
 *
 * String ownership: every char* returned via an out-pointer is heap-allocated
 * by the engine. Callers MUST free it with dune_free(). Strings returned by
 * value (const char* from dune_api_version etc.) are static and not freed.
 *
 * Threading: a session is single-threaded from the caller's perspective. Two
 * threads must not call any dune_session_* function on the same handle
 * concurrently. Different sessions can be used from different threads.
 *
 * Return-code convention:
 *   DUNE_OK        (0)  : success
 *   DUNE_DONE      (1)  : the game ended (call destroy)
 *   DUNE_PENDING   (2)  : v2 — a decision is awaiting submit
 *   DUNE_NO_EVENT  (3)  : poll exhausted (no event was returned)
 *   DUNE_ERR_ARG   (-1) : bad argument (null handle, out-of-range index)
 *   DUNE_ERR_STATE (-2) : called in wrong state (e.g. submit without pending)
 *   DUNE_ERR_INTERNAL (-3): caught exception inside the engine
 */

#ifndef DUNE_C_API_H
#define DUNE_C_API_H

/* Per-symbol export marker. On Windows the C ABI surface is annotated with
 * __declspec(dllexport) when building libdune.dll, and __declspec(dllimport)
 * when consuming it. This replaces -Wl,--export-all-symbols, which spilled
 * embedded-static dependencies (winpthread, libstdc++) into libdune.dll.a
 * and caused duplicate-symbol link errors in downstream consumers like the
 * GDExtension wrapper.
 *
 * Define DUNE_BUILDING_DLL when compiling the engine on Windows; consumers
 * leave it unset and pick up dllimport. On Linux/macOS the macro is empty —
 * default visibility already exports extern "C" symbols. */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef DUNE_BUILDING_DLL
    #define DUNE_API __declspec(dllexport)
  #else
    #define DUNE_API __declspec(dllimport)
  #endif
#else
  #define DUNE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DUNE_OK             0
#define DUNE_DONE           1
#define DUNE_PENDING        2
#define DUNE_NO_EVENT       3
#define DUNE_ERR_ARG       -1
#define DUNE_ERR_STATE     -2
#define DUNE_ERR_INTERNAL  -3

typedef struct dune_session dune_session_t;

/* Returns a static string of the form "1.0" or "1.0+pr4b". Not freed. */
DUNE_API const char* dune_api_version(void);

/* Returns build metadata: compiler, flags, schema version. Not freed. */
DUNE_API const char* dune_api_build_info(void);

/* Allocates a new session. Returns NULL on failure (out of memory, bad args).
 * num_players: 2-6. seed: any uint32. The created session has run no phases. */
DUNE_API dune_session_t* dune_session_create(unsigned int seed, int num_players);

/* Tears down the session. After this, the handle must not be used.
 * Safe to call with NULL (no-op). */
DUNE_API void dune_session_destroy(dune_session_t* session);

/* v1 synchronous run. Advances the game from its current state until either
 * the game ends or a decision is required (the latter is impossible in v1
 * because the FFI session uses an AI adapter; v2 changes that). Returns
 * DUNE_DONE on completion, DUNE_ERR_INTERNAL on engine exception. */
DUNE_API int dune_session_run_to_end(dune_session_t* session);

/* Builds a JSON document describing the full game state. *out_json must be
 * freed by the caller via dune_free(). Returns DUNE_OK or an error code.
 * Hidden information is NOT redacted — server-side filter is responsible. */
DUNE_API int dune_session_get_snapshot(dune_session_t* session, char** out_json);

/* Pops the next event from the session's queue. The session subscribes to
 * the engine's event bus on creation; events emitted during run_to_end are
 * accumulated in publish order. Returns DUNE_OK if an event was returned
 * (caller frees *out_json), DUNE_NO_EVENT if the queue is empty. */
DUNE_API int dune_session_poll_event(dune_session_t* session, char** out_json);

/* Frees a string previously returned by a dune_session_* call.
 * Safe to call with NULL. */
DUNE_API void dune_free(char* p);

/* ---- v2: interactive session (PR 4b) ----
 *
 * Interactive sessions install an FFIAsyncAdapter inside the engine. When
 * the engine reaches a decision site, the worker thread parks on a condvar
 * and dune_session_step returns DUNE_PENDING; the host then reads the
 * pending request, submits a response, and continues.
 *
 * v1 sessions (created via dune_session_create) are AI-mode and reject
 * step/get_pending_decision/submit_decision with DUNE_ERR_STATE.
 * v2 sessions reject dune_session_run_to_end with DUNE_ERR_STATE.
 *
 * Cancellation: dune_session_destroy on an interactive session cooperatively
 * signals the worker. If a decision is pending, the adapter throws
 * SessionCancelled inside the worker thread; the worker function catches it
 * and exits. destroy then joins. No detach, no leak.
 */

/* Allocates a new interactive session. Returns NULL on failure.
 * Same constraints as dune_session_create (num_players: 2-6). */
DUNE_API dune_session_t* dune_session_create_interactive(unsigned int seed, int num_players);

/* Advance the engine until the next decision request or game completion.
 * Behaviour by current state:
 *   Idle             → spawns the worker thread, then waits for the first
 *                      stable transition.
 *   Running          → waits for the worker to reach a stable state.
 *   AwaitingDecision → returns DUNE_ERR_STATE (host must submit first).
 *   Done             → returns DUNE_DONE.
 *   Error            → returns DUNE_ERR_INTERNAL.
 *
 * Returns DUNE_PENDING if a decision is now waiting, DUNE_DONE if the game
 * ended, DUNE_ERR_STATE if called on an AI-mode session or with a pending
 * decision unanswered, DUNE_ERR_ARG on bad handle, DUNE_ERR_INTERNAL on
 * worker exception. */
DUNE_API int dune_session_step(dune_session_t* session);

/* Read the JSON-serialized DecisionRequest currently awaiting a response.
 * *out_json must be freed by the caller via dune_free. Returns DUNE_OK,
 * DUNE_ERR_STATE if no decision is pending, or DUNE_ERR_ARG on bad handle. */
DUNE_API int dune_session_get_pending_decision(dune_session_t* session, char** out_json);

/* Provide the response to the pending decision and advance the engine.
 * in_json must be {"value": "<string>"} where the string becomes
 * DecisionResponse.payload_json verbatim (the engine's existing parsing
 * consumes it for both simple and compound kinds).
 *
 * Returns the same codes as dune_session_step once the engine stabilises
 * after consuming the response (DUNE_PENDING / DUNE_DONE / DUNE_ERR_*). */
DUNE_API int dune_session_submit_decision(dune_session_t* session, const char* in_json);

#ifdef __cplusplus
}
#endif

#endif /* DUNE_C_API_H */
