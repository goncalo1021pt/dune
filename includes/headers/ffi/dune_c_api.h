/* dune_c_api.h — C ABI for libdune.so, v1 (PR 4a slice).
 *
 * This is the frozen surface that Godot/GDExtension links against. Everything
 * exported here must keep its signature stable — additions are fine, but
 * existing function signatures must not change without a major version bump.
 *
 * v1 (PR 4a, this file): synchronous run + read-only snapshot/event polling.
 *   Sufficient to drive headless replay/visualisation in Godot. Adequate for
 *   single-player CLI parity.
 *
 * v2 (PR 4b, planned):  non-blocking step + decision submit. Adds:
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
const char* dune_api_version(void);

/* Returns build metadata: compiler, flags, schema version. Not freed. */
const char* dune_api_build_info(void);

/* Allocates a new session. Returns NULL on failure (out of memory, bad args).
 * num_players: 2-6. seed: any uint32. The created session has run no phases. */
dune_session_t* dune_session_create(unsigned int seed, int num_players);

/* Tears down the session. After this, the handle must not be used.
 * Safe to call with NULL (no-op). */
void dune_session_destroy(dune_session_t* session);

/* v1 synchronous run. Advances the game from its current state until either
 * the game ends or a decision is required (the latter is impossible in v1
 * because the FFI session uses an AI adapter; v2 changes that). Returns
 * DUNE_DONE on completion, DUNE_ERR_INTERNAL on engine exception. */
int dune_session_run_to_end(dune_session_t* session);

/* Builds a JSON document describing the full game state. *out_json must be
 * freed by the caller via dune_free(). Returns DUNE_OK or an error code.
 * Hidden information is NOT redacted — server-side filter is responsible. */
int dune_session_get_snapshot(dune_session_t* session, char** out_json);

/* Pops the next event from the session's queue. The session subscribes to
 * the engine's event bus on creation; events emitted during run_to_end are
 * accumulated in publish order. Returns DUNE_OK if an event was returned
 * (caller frees *out_json), DUNE_NO_EVENT if the queue is empty. */
int dune_session_poll_event(dune_session_t* session, char** out_json);

/* Frees a string previously returned by a dune_session_* call.
 * Safe to call with NULL. */
void dune_free(char* p);

#ifdef __cplusplus
}
#endif

#endif /* DUNE_C_API_H */
