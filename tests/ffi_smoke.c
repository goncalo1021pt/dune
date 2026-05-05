/* ffi_smoke.c — pure-C smoke test for libdune.so.
 *
 * Exercises the v1 surface end-to-end: version, build info, create, get
 * snapshot at start, run-to-end, poll all events, get final snapshot,
 * destroy. If any step fails the program exits non-zero.
 *
 * Built and run via `make ffi_smoke`. Linker resolves dune_* symbols from
 * libdune.so in the build directory; LD_LIBRARY_PATH=. lets the loader
 * find it without an install step.
 */

#include "ffi/dune_c_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
	fprintf(stderr, "[ffi_smoke] FAIL: %s\n", msg);
	return 1;
}

int main(void) {
	const char* version = dune_api_version();
	const char* build   = dune_api_build_info();
	if (!version || !build) return fail("api strings are NULL");
	printf("[ffi_smoke] version=%s\n", version);
	printf("[ffi_smoke] build=%s\n",   build);

	dune_session_t* s = dune_session_create(/*seed=*/42, /*num_players=*/6);
	if (!s) return fail("session_create returned NULL");

	/* Snapshot before any phase has run — must contain schema_version=1 and
	 * the "game" block with turn 0. We don't parse JSON in C; just sanity
	 * check the substring is there. */
	char* snap0 = NULL;
	int rc = dune_session_get_snapshot(s, &snap0);
	if (rc != DUNE_OK || !snap0) {
		dune_session_destroy(s);
		return fail("get_snapshot before run failed");
	}
	if (!strstr(snap0, "\"schema_version\":1")) {
		dune_free(snap0);
		dune_session_destroy(s);
		return fail("initial snapshot missing schema_version");
	}
	dune_free(snap0);

	/* Synchronous run. AI plays both sides; no decision loop needed in v1. */
	rc = dune_session_run_to_end(s);
	if (rc != DUNE_DONE) {
		dune_session_destroy(s);
		return fail("run_to_end did not return DUNE_DONE");
	}

	/* Drain events. We expect a non-trivial number — every storm move,
	 * battle, spice blow, etc. emits one. We just verify the queue empties. */
	int events_polled = 0;
	for (;;) {
		char* ev = NULL;
		rc = dune_session_poll_event(s, &ev);
		if (rc == DUNE_NO_EVENT) break;
		if (rc != DUNE_OK || !ev) {
			dune_session_destroy(s);
			return fail("poll_event returned bad rc");
		}
		++events_polled;
		dune_free(ev);
		if (events_polled > 100000) {
			/* defensive: catch a runaway emitter so the test doesn't hang */
			dune_session_destroy(s);
			return fail("event queue did not drain (>100k events)");
		}
	}
	printf("[ffi_smoke] drained %d events\n", events_polled);
	if (events_polled == 0) {
		dune_session_destroy(s);
		return fail("expected non-zero events from a full game run");
	}

	/* Final snapshot must report game_ended=true. */
	char* snap_end = NULL;
	rc = dune_session_get_snapshot(s, &snap_end);
	if (rc != DUNE_OK || !snap_end) {
		dune_session_destroy(s);
		return fail("final snapshot failed");
	}
	if (!strstr(snap_end, "\"game_ended\":true")) {
		fprintf(stderr, "[ffi_smoke] snap_end=%.200s...\n", snap_end);
		dune_free(snap_end);
		dune_session_destroy(s);
		return fail("final snapshot did not report game_ended=true");
	}
	dune_free(snap_end);

	/* dune_free(NULL) must be a no-op. */
	dune_free(NULL);

	dune_session_destroy(s);
	dune_session_destroy(NULL); /* must be no-op */

	printf("[ffi_smoke] PASS\n");
	return 0;
}
