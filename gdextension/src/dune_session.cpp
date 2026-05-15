#include "dune_session.h"

#include "ffi/dune_c_api.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot_dune {

using namespace godot;

DuneSession::DuneSession() = default;

DuneSession::~DuneSession() {
	destroy();
}

void DuneSession::_bind_methods() {
	ClassDB::bind_static_method("DuneSession", D_METHOD("api_version"), &DuneSession::api_version);
	ClassDB::bind_static_method("DuneSession", D_METHOD("build_info"), &DuneSession::build_info);

	ClassDB::bind_method(D_METHOD("create", "seed", "num_players"), &DuneSession::create);
	ClassDB::bind_method(D_METHOD("create_interactive", "seed", "num_players"), &DuneSession::create_interactive);
	ClassDB::bind_method(D_METHOD("destroy"), &DuneSession::destroy);
	ClassDB::bind_method(D_METHOD("is_open"), &DuneSession::is_open);
	ClassDB::bind_method(D_METHOD("is_interactive"), &DuneSession::is_interactive);

	ClassDB::bind_method(D_METHOD("run_to_end"), &DuneSession::run_to_end);
	ClassDB::bind_method(D_METHOD("get_snapshot"), &DuneSession::get_snapshot);
	ClassDB::bind_method(D_METHOD("poll_event"), &DuneSession::poll_event);

	ClassDB::bind_method(D_METHOD("step"), &DuneSession::step);
	ClassDB::bind_method(D_METHOD("get_pending_decision"), &DuneSession::get_pending_decision);
	ClassDB::bind_method(D_METHOD("submit_decision", "response_json"), &DuneSession::submit_decision);

	BIND_ENUM_CONSTANT(RESULT_OK);
	BIND_ENUM_CONSTANT(RESULT_DONE);
	BIND_ENUM_CONSTANT(RESULT_PENDING);
	BIND_ENUM_CONSTANT(RESULT_NO_EVENT);
	BIND_ENUM_CONSTANT(RESULT_ERR_ARG);
	BIND_ENUM_CONSTANT(RESULT_ERR_STATE);
	BIND_ENUM_CONSTANT(RESULT_ERR_INTERNAL);
}

String DuneSession::take_owned_cstring(char *p) {
	if (p == nullptr) {
		return String();
	}
	String s = String::utf8(p);
	dune_free(p);
	return s;
}

String DuneSession::api_version() {
	const char *v = dune_api_version();
	return v ? String::utf8(v) : String();
}

String DuneSession::build_info() {
	const char *v = dune_api_build_info();
	return v ? String::utf8(v) : String();
}

bool DuneSession::create(uint32_t seed, int num_players) {
	if (handle != nullptr) {
		return false;
	}
	handle = dune_session_create(seed, num_players);
	interactive = false;
	return handle != nullptr;
}

bool DuneSession::create_interactive(uint32_t seed, int num_players) {
	if (handle != nullptr) {
		return false;
	}
	handle = dune_session_create_interactive(seed, num_players);
	interactive = true;
	return handle != nullptr;
}

void DuneSession::destroy() {
	if (handle != nullptr) {
		dune_session_destroy(handle);
		handle = nullptr;
	}
	interactive = false;
}

bool DuneSession::is_open() const {
	return handle != nullptr;
}

bool DuneSession::is_interactive() const {
	return interactive;
}

int DuneSession::run_to_end() {
	if (handle == nullptr) {
		return RESULT_ERR_ARG;
	}
	return dune_session_run_to_end(handle);
}

String DuneSession::get_snapshot() {
	if (handle == nullptr) {
		return String();
	}
	char *out = nullptr;
	int rc = dune_session_get_snapshot(handle, &out);
	if (rc != DUNE_OK) {
		return String();
	}
	return take_owned_cstring(out);
}

String DuneSession::poll_event() {
	if (handle == nullptr) {
		return String();
	}
	char *out = nullptr;
	int rc = dune_session_poll_event(handle, &out);
	if (rc != DUNE_OK) {
		return String();
	}
	return take_owned_cstring(out);
}

int DuneSession::step() {
	if (handle == nullptr) {
		return RESULT_ERR_ARG;
	}
	return dune_session_step(handle);
}

String DuneSession::get_pending_decision() {
	if (handle == nullptr) {
		return String();
	}
	char *out = nullptr;
	int rc = dune_session_get_pending_decision(handle, &out);
	if (rc != DUNE_OK) {
		return String();
	}
	return take_owned_cstring(out);
}

int DuneSession::submit_decision(const String &response_json) {
	if (handle == nullptr) {
		return RESULT_ERR_ARG;
	}
	CharString utf8 = response_json.utf8();
	return dune_session_submit_decision(handle, utf8.get_data());
}

} // namespace godot_dune
