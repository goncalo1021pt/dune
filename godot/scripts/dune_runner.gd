extends Control

# Drives a DuneSession and hands its snapshots to the map view + side panel.
#
# Two modes:
#   - "Run game" (AI): synchronous run_to_end() — engine plays itself to
#     completion, we render the final snapshot.
#   - "Play interactive": opens an interactive session (v2 ABI) and steps
#     decision-by-decision. Every actor's prompts route through the
#     DecisionPanel, so the player drives all factions hot-seat style.
#     Per-actor AI/human routing is a follow-up (and the seam where
#     multiplayer enters).

@onready var status_label: Label    = $HBox/Side/Status
@onready var seed_spin: SpinBox     = $HBox/Side/Controls/SeedSpin
@onready var players_spin: SpinBox  = $HBox/Side/Controls/PlayersSpin
@onready var run_button: Button     = $HBox/Side/Buttons/RunButton
@onready var play_button: Button    = $HBox/Side/Buttons/PlayButton
@onready var event_log: RichTextLabel = $HBox/Side/EventLog
@onready var summary_label: Label   = $HBox/Side/Summary
@onready var map_view: DuneMapView  = $HBox/Map
@onready var decision_panel: PanelContainer = $HBox/Side/Decision

var _session: DuneSession = null
# Rolling tail of human-readable event lines, kept across interactive
# decisions so the side panel reads like a phase log.
var _event_tail: Array[String] = []
const _EVENT_TAIL_LIMIT := 30
# Total events drained over the lifetime of the current session — useful
# for showing progress in interactive mode where the game ends after
# many small steps.
var _event_count: int = 0


func _ready() -> void:
	run_button.pressed.connect(_on_run_pressed)
	play_button.pressed.connect(_on_play_interactive_pressed)
	decision_panel.decision_submitted.connect(_on_decision_submitted)
	seed_spin.value = 42
	players_spin.value = 6
	status_label.text = "libdune API %s" % DuneSession.api_version()


# --- AI mode (unchanged from PR #36) ---------------------------------

func _on_run_pressed() -> void:
	_reset_state("Running…")
	# Defer one frame so the UI repaints "Running…" before the engine
	# blocks the main thread inside run_to_end().
	await get_tree().process_frame
	_run_session_ai()


func _run_session_ai() -> void:
	_session = DuneSession.new()
	if not _session.create(int(seed_spin.value), int(players_spin.value)):
		status_label.text = "FAIL: dune_session_create returned NULL"
		return

	var rc: int = _session.run_to_end()
	if rc != DuneSession.RESULT_DONE:
		status_label.text = "FAIL: run_to_end rc=%d" % rc
		return

	_drain_events()
	_refresh_render()
	status_label.text = "Done — drained %d events." % _event_count


# --- Interactive mode -------------------------------------------------

func _on_play_interactive_pressed() -> void:
	_reset_state("Starting interactive session…")
	await get_tree().process_frame
	_session = DuneSession.new()
	if not _session.create_interactive(int(seed_spin.value), int(players_spin.value)):
		status_label.text = "FAIL: dune_session_create_interactive returned NULL"
		return
	_advance()


# Step the engine forward to its next stable state and route accordingly.
# Called once after create_interactive(), then again after every
# decision the player submits.
func _advance() -> void:
	if _session == null or not _session.is_open():
		return
	# Yield so the UI repaints whatever the previous decision changed
	# before we re-enter the blocking step() call.
	await get_tree().process_frame

	var rc: int = _session.step()
	_drain_events()
	_refresh_render()

	match rc:
		DuneSession.RESULT_PENDING:
			_show_pending_decision()
		DuneSession.RESULT_DONE:
			status_label.text = "Game over — drained %d events." % _event_count
		DuneSession.RESULT_ERR_STATE:
			status_label.text = "FAIL: step rc=ERR_STATE (worker not idle?)"
		_:
			status_label.text = "FAIL: step rc=%d" % rc


func _on_decision_submitted(response_json: String) -> void:
	if _session == null or not _session.is_open():
		return
	status_label.text = "Submitting decision…"
	await get_tree().process_frame
	var rc: int = _session.submit_decision(response_json)
	_drain_events()
	_refresh_render()

	match rc:
		DuneSession.RESULT_PENDING:
			_show_pending_decision()
		DuneSession.RESULT_DONE:
			status_label.text = "Game over — drained %d events." % _event_count
		_:
			status_label.text = "FAIL: submit_decision rc=%d" % rc


# Pull the pending decision JSON, look up the actor's faction name from
# the snapshot, hand both to the decision panel.
func _show_pending_decision() -> void:
	var request_json: String = _session.get_pending_decision()
	if request_json.is_empty():
		status_label.text = "FAIL: get_pending_decision returned empty"
		return
	var parsed: Variant = JSON.parse_string(request_json)
	if typeof(parsed) != TYPE_DICTIONARY:
		status_label.text = "FAIL: decision JSON didn't parse"
		return
	var request: Dictionary = parsed
	var actor_index: int = int(request.get("actor_index", -1))
	var actor_label: String = _faction_name_for(actor_index)
	status_label.text = "Waiting for %s…" % actor_label
	decision_panel.show_request(request, actor_label)


# --- Shared helpers --------------------------------------------------

func _reset_state(new_status: String) -> void:
	status_label.text = new_status
	event_log.text = ""
	summary_label.text = ""
	_event_tail.clear()
	_event_count = 0
	decision_panel.clear()


# Drain everything currently in the engine's event queue into the
# rolling tail. Cheap to call between decisions — the FFI poll returns
# DUNE_NO_EVENT (and an empty string) as soon as the queue is dry.
func _drain_events() -> void:
	while true:
		var ev_json := _session.poll_event()
		if ev_json.is_empty():
			break
		_event_count += 1
		var summary := _summarise_event(ev_json)
		if not summary.is_empty():
			_event_tail.append(summary)
			if _event_tail.size() > _EVENT_TAIL_LIMIT:
				_event_tail.pop_front()
	event_log.text = "\n".join(_event_tail)


# Pull a fresh snapshot, re-render the map and the summary panel.
func _refresh_render() -> void:
	var snapshot_json := _session.get_snapshot()
	var parsed: Variant = JSON.parse_string(snapshot_json)
	if typeof(parsed) != TYPE_DICTIONARY:
		return
	var snapshot: Dictionary = parsed
	map_view.set_snapshot(snapshot)
	_render_summary(snapshot)


func _faction_name_for(actor_index: int) -> String:
	if actor_index < 0 or _session == null:
		return "?"
	var snapshot_json := _session.get_snapshot()
	var parsed: Variant = JSON.parse_string(snapshot_json)
	if typeof(parsed) != TYPE_DICTIONARY:
		return "actor #%d" % actor_index
	var players: Array = (parsed as Dictionary).get("players", [])
	for p in players:
		if int(p.get("faction_index", -1)) == actor_index:
			return str(p.get("faction_name", "actor #%d" % actor_index))
	return "actor #%d" % actor_index


# Pull a one-line summary from a JSON event. The event schema is documented
# in includes/Docs/snapshot_schema.md (Event section).
func _summarise_event(json_text: String) -> String:
	var parsed: Variant = JSON.parse_string(json_text)
	if typeof(parsed) != TYPE_DICTIONARY:
		return ""
	var ev: Dictionary = parsed
	var t: String = str(ev.get("turn", "?"))
	var phase: String = str(ev.get("phase", ""))
	var faction: String = str(ev.get("player_faction", ""))
	var msg: String = str(ev.get("message", ""))
	var faction_part := "" if faction.is_empty() else " %s" % faction
	return "[T%s %s%s] %s" % [t, phase, faction_part, msg]


func _render_summary(snapshot: Dictionary) -> void:
	var game: Dictionary = snapshot.get("game", {})
	var players: Array = snapshot.get("players", [])
	var lines: Array[String] = []
	lines.append("Turn %d / phase %s%s" % [
		int(game.get("turn", 0)),
		str(game.get("phase", "?")),
		"  ENDED" if bool(game.get("game_ended", false)) else "",
	])
	lines.append("Seed %d, players %d" % [
		int(game.get("initial_seed", 0)),
		int(game.get("player_count", 0)),
	])
	lines.append("Storm sector: %d" % int(snapshot.get("storm", {}).get("sector", 0)))
	lines.append("Events drained: %d" % _event_count)
	lines.append("")
	lines.append("Spice:")
	for p in players:
		lines.append("  %-14s  %d" % [str(p.get("faction_name", "?")), int(p.get("spice", 0))])
	summary_label.text = "\n".join(lines)
