/* ffi_smoke_interactive.c — pure-C smoke for the v2 FFI surface (PR 4b).
 *
 * Exercises every new symbol: dune_session_create_interactive, step,
 * get_pending_decision, submit_decision. Plus the cross-mode error
 * contracts (AI sessions reject v2 symbols, interactive sessions reject
 * run_to_end). Plus destroy-mid-decision cooperative cancel.
 *
 * The game-driving policy is intentionally minimal: yn → "n" (decline),
 * int → int_min, select → first option, deployment/movement → skip. The
 * point is to prove the threading model and ABI work end-to-end, not to
 * play the game well. Seed 42 still ends with the turn-10 SPECIAL VICTORY
 * because no faction can claim 3 cities under "skip everything" play.
 *
 * Built and run via `make ffi_smoke_interactive`. Same LD_LIBRARY_PATH=.
 * trick as ffi_smoke.c.
 */

#include "ffi/dune_c_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
	fprintf(stderr, "[ffi_smoke_interactive] FAIL: %s\n", msg);
	return 1;
}

/* Decide what {"value": "..."} JSON to submit for a given pending request.
 * Naive substring parsing — fine for our well-controlled JSON output.
 *
 * PR 4c: only primitive kinds remain (yn / int / select). Compound deployment
 * and movement decisions are now driven by the engine as sequences of these
 * primitives, so the policy is uniformly "pick the minimum that progresses":
 *   yn     -> "n"  (decline)
 *   int    -> int_min (typically 0; for unit-count prompts that means skip)
 *   select -> "" if allow_none is true (skip), else first option */
static void craft_response(const char* req_json, char* out, size_t out_size) {
	const char* kind_pos = strstr(req_json, "\"kind\":\"");
	if (!kind_pos) { snprintf(out, out_size, "{\"value\":\"\"}"); return; }
	kind_pos += 8;  /* past `"kind":"` */

	if (strncmp(kind_pos, "yn\"", 3) == 0) {
		snprintf(out, out_size, "{\"value\":\"n\"}");
		return;
	}
	if (strncmp(kind_pos, "int\"", 4) == 0) {
		const char* min_pos = strstr(req_json, "\"int_min\":");
		int min_val = 0;
		if (min_pos) sscanf(min_pos + 10, "%d", &min_val);
		snprintf(out, out_size, "{\"value\":\"%d\"}", min_val);
		return;
	}
	if (strncmp(kind_pos, "select\"", 7) == 0) {
		/* Honour allow_none: if true, submit empty value to skip. This is
		 * how the engine signals optional decisions in the new primitive
		 * flow (e.g. "choose territory or skip" at the start of deployment). */
		if (strstr(req_json, "\"allow_none\":true")) {
			snprintf(out, out_size, "{\"value\":\"\"}");
			return;
		}
		/* Mandatory select: pick the first option. */
		const char* opts_pos = strstr(req_json, "\"options\":[\"");
		if (opts_pos) {
			opts_pos += 12;
			const char* end = strchr(opts_pos, '"');
			if (end) {
				size_t len = (size_t)(end - opts_pos);
				if (len < out_size - 32) {
					snprintf(out, out_size, "{\"value\":\"%.*s\"}", (int)len, opts_pos);
					return;
				}
			}
		}
		snprintf(out, out_size, "{\"value\":\"\"}");
		return;
	}
	snprintf(out, out_size, "{\"value\":\"\"}");
}

/* Drive a session through a full game with the minimal policy above. */
static int drive_to_completion(dune_session_t* s, int max_iter) {
	int rc = dune_session_step(s);
	if (rc == DUNE_DONE) return DUNE_DONE;
	if (rc != DUNE_PENDING) return rc;

	for (int i = 0; i < max_iter; ++i) {
		char* req = NULL;
		rc = dune_session_get_pending_decision(s, &req);
		if (rc != DUNE_OK || !req) return DUNE_ERR_INTERNAL;

		char buf[256];
		craft_response(req, buf, sizeof(buf));
		dune_free(req);

		rc = dune_session_submit_decision(s, buf);
		if (rc == DUNE_DONE) return DUNE_DONE;
		if (rc != DUNE_PENDING) return rc;
	}
	return DUNE_ERR_INTERNAL;
}

int main(void) {
	/* --- Test 1: drive a full game via the v2 surface. --- */
	{
		dune_session_t* s = dune_session_create_interactive(42, 6);
		if (!s) return fail("create_interactive returned NULL");

		int rc = drive_to_completion(s, 100000);
		if (rc != DUNE_DONE) {
			dune_session_destroy(s);
			return fail("drive_to_completion did not reach DUNE_DONE");
		}

		/* Final snapshot should report the game ended. */
		char* snap = NULL;
		if (dune_session_get_snapshot(s, &snap) != DUNE_OK || !snap) {
			dune_session_destroy(s);
			return fail("final snapshot failed");
		}
		if (!strstr(snap, "\"game_ended\":true")) {
			dune_free(snap);
			dune_session_destroy(s);
			return fail("final snapshot did not report game_ended=true");
		}
		dune_free(snap);
		dune_session_destroy(s);
		printf("[ffi_smoke_interactive] full game drive: OK\n");
	}

	/* --- Test 2: destroy mid-decision (cooperative cancel). --- */
	{
		dune_session_t* s = dune_session_create_interactive(42, 6);
		if (!s) return fail("test2: create returned NULL");

		int rc = dune_session_step(s);
		if (rc != DUNE_PENDING) {
			dune_session_destroy(s);
			return fail("test2: expected DUNE_PENDING on first step");
		}
		/* Do NOT submit. Destroy must cooperatively cancel and join. */
		dune_session_destroy(s);
		printf("[ffi_smoke_interactive] destroy mid-decision: OK\n");
	}

	/* --- Test 3: error states for the v2 surface. --- */
	{
		dune_session_t* s = dune_session_create_interactive(42, 6);
		if (!s) return fail("test3: create returned NULL");

		/* get_pending_decision before any step → ERR_STATE (Idle, not waiting). */
		char* j = NULL;
		if (dune_session_get_pending_decision(s, &j) != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test3: get_pending before step should ERR_STATE");
		}
		/* submit_decision before any step → ERR_STATE. */
		if (dune_session_submit_decision(s, "{\"value\":\"y\"}") != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test3: submit before step should ERR_STATE");
		}
		/* Bad in_json → ERR_ARG (after step has set state to AwaitingDecision). */
		int rc = dune_session_step(s);
		if (rc != DUNE_PENDING) { dune_session_destroy(s); return fail("test3: step should be PENDING"); }
		if (dune_session_submit_decision(s, "not json") != DUNE_ERR_ARG) {
			dune_session_destroy(s);
			return fail("test3: malformed submit should ERR_ARG");
		}
		/* step() while awaiting decision → ERR_STATE. */
		if (dune_session_step(s) != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test3: step while awaiting should ERR_STATE");
		}
		dune_session_destroy(s);
		printf("[ffi_smoke_interactive] error states: OK\n");
	}

	/* --- Test 4: AI-mode session refuses v2 symbols. --- */
	{
		dune_session_t* s = dune_session_create(42, 6);
		if (!s) return fail("test4: AI create returned NULL");

		if (dune_session_step(s) != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test4: AI session should refuse step");
		}
		char* j = NULL;
		if (dune_session_get_pending_decision(s, &j) != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test4: AI session should refuse get_pending");
		}
		if (dune_session_submit_decision(s, "{\"value\":\"y\"}") != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test4: AI session should refuse submit");
		}
		dune_session_destroy(s);
		printf("[ffi_smoke_interactive] AI mode rejects v2: OK\n");
	}

	/* --- Test 5: interactive session refuses run_to_end. --- */
	{
		dune_session_t* s = dune_session_create_interactive(42, 6);
		if (!s) return fail("test5: create returned NULL");
		if (dune_session_run_to_end(s) != DUNE_ERR_STATE) {
			dune_session_destroy(s);
			return fail("test5: interactive should refuse run_to_end");
		}
		dune_session_destroy(s);
		printf("[ffi_smoke_interactive] interactive rejects run_to_end: OK\n");
	}

	printf("[ffi_smoke_interactive] PASS\n");
	return 0;
}
