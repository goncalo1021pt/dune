#ifndef DUNE_GDEXTENSION_SESSION_H
#define DUNE_GDEXTENSION_SESSION_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

extern "C" {
struct dune_session;
typedef struct dune_session dune_session_t;
}

namespace godot_dune {

// Thin Godot-side wrapper around the 12-symbol libdune C ABI.
//
// One DuneSession owns one libdune session handle. Lifetime is RefCounted —
// the destructor calls dune_session_destroy if a handle is still open, so a
// script can drop the reference without leaking the engine session (or its
// worker thread, in interactive mode).
//
// The class is mode-aware: create() opens an AI-mode session (v1 ABI),
// create_interactive() opens an interactive one (v2 ABI). Calling a v2
// method on an AI session — or run_to_end() on an interactive session —
// surfaces DUNE_ERR_STATE from the engine, mirroring the C ABI contract.
class DuneSession : public godot::RefCounted {
	GDCLASS(DuneSession, godot::RefCounted)

public:
	// Mirrors the DUNE_* return codes from dune_c_api.h so GDScript can
	// compare against them without having to know the underlying values.
	enum ResultCode {
		RESULT_OK = 0,
		RESULT_DONE = 1,
		RESULT_PENDING = 2,
		RESULT_NO_EVENT = 3,
		RESULT_ERR_ARG = -1,
		RESULT_ERR_STATE = -2,
		RESULT_ERR_INTERNAL = -3,
	};

	DuneSession();
	~DuneSession();

	static godot::String api_version();
	static godot::String build_info();

	// Returns true on success. After a successful call, is_open() == true.
	// Calling create*() on an already-open session is rejected (returns false).
	bool create(uint32_t seed, int num_players);
	bool create_interactive(uint32_t seed, int num_players);

	// Idempotent — safe on a closed session, safe to call twice.
	void destroy();

	bool is_open() const;
	bool is_interactive() const;

	// v1 surface.
	int run_to_end();
	godot::String get_snapshot();
	// Returns "" when the queue is empty; the engine never emits empty JSON,
	// so the empty-string sentinel is sufficient for normal use.
	godot::String poll_event();

	// v2 surface.
	int step();
	godot::String get_pending_decision();
	int submit_decision(const godot::String &response_json);

protected:
	static void _bind_methods();

private:
	dune_session_t *handle = nullptr;
	bool interactive = false;

	// Takes ownership of a libdune-allocated char*, copies it into a Godot
	// String, frees the original via dune_free.
	static godot::String take_owned_cstring(char *p);
};

} // namespace godot_dune

VARIANT_ENUM_CAST(godot_dune::DuneSession::ResultCode);

#endif // DUNE_GDEXTENSION_SESSION_H
