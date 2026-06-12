extends Control

# Drives a DuneSession and hands its snapshots to the map view + side panel.
#
# Two modes share the same scene:
#   AI run         — synchronous run_to_end; renders the final state.
#   Interactive    — create_interactive, then a step/submit loop:
#                      while step() returns PENDING:
#                          get_pending_decision → render in decision panel
#                          await user submit    → submit_decision
#                    A snapshot refresh happens after every submit so the
#                    map updates between every decision the player makes.
#
# The skin selector picks any *.json under res://skins/. Loading a skin with
# a map.image flips the map view from procedural polar drawing to image-
# underlay rendering; with everything else (faction tokens, anchors) layered
# on top per the skin's settings.

# preload() the script-defined classes so their types resolve on the first
# parse pass — class_name registration happens later in Godot's compile
# sequence and any direct DuneSkin / DuneMapView / DuneDecisionPanel
# reference here would fail until the second pass.
const DuneSkin           := preload("res://scripts/dune_skin.gd")
const DuneMapView        := preload("res://scripts/dune_map_view.gd")
const DuneDecisionPanel  := preload("res://scripts/dune_decision_panel.gd")

@onready var status_label: Label              = $HBox/Side/Status
@onready var seed_spin: SpinBox               = $HBox/Side/Controls/SeedSpin
@onready var players_spin: SpinBox            = $HBox/Side/Controls/PlayersSpin
@onready var skin_picker: OptionButton        = $HBox/Side/Controls/SkinPicker
@onready var run_button: Button               = $HBox/Side/Buttons/RunButton
@onready var play_button: Button              = $HBox/Side/Buttons/PlayButton
@onready var stop_button: Button              = $HBox/Side/Buttons/StopButton
@onready var event_log: RichTextLabel         = $HBox/Side/EventLog
@onready var summary_label: Label             = $HBox/Side/Summary
@onready var map_view                          = $HBox/Map           # DuneMapView
@onready var decision_panel                    = $HBox/Side/DecisionPanel  # DuneDecisionPanel

var _session: DuneSession = null
var _skin_paths: Array[String] = []
var _interactive_running := false
var _stop_requested := false

func _ready() -> void:
	run_button.pressed.connect(_on_run_pressed)
	play_button.pressed.connect(_on_play_pressed)
	stop_button.pressed.connect(_on_stop_pressed)
	seed_spin.value = 42
	players_spin.value = 6
	stop_button.disabled = true

	_populate_skin_picker()
	skin_picker.item_selected.connect(_on_skin_selected)
	_apply_selected_skin()

	status_label.text = "libdune API %s" % DuneSession.api_version()

# --- Skin selector ---

func _populate_skin_picker() -> void:
	skin_picker.clear()
	skin_picker.add_item("(none — procedural)")  # idx 0
	_skin_paths = DuneSkin.list_skin_files()
	for p in _skin_paths:
		skin_picker.add_item(p.get_file().get_basename())
	if not _skin_paths.is_empty():
		skin_picker.selected = 1   # default to first real skin

func _on_skin_selected(_idx: int) -> void:
	_apply_selected_skin()

func _apply_selected_skin() -> void:
	var idx := skin_picker.selected
	if idx <= 0 or idx - 1 >= _skin_paths.size():
		map_view.set_skin(null)
		return
	var path := _skin_paths[idx - 1]
	var skin := DuneSkin.load_from_file(path)
	map_view.set_skin(skin)

# --- AI run ---

func _on_run_pressed() -> void:
	if _interactive_running:
		return
	_destroy_session()
	status_label.text = "Running…"
	event_log.text = ""
	summary_label.text = ""
	# Defer a frame so the UI repaints before run_to_end blocks the thread.
	await get_tree().process_frame

	_session = DuneSession.new()
	if not _session.create(int(seed_spin.value), int(players_spin.value)):
		status_label.text = "FAIL: dune_session_create returned NULL"
		return

	var rc: int = _session.run_to_end()
	if rc != DuneSession.RESULT_DONE:
		status_label.text = "FAIL: run_to_end rc=%d" % rc
		return

	var event_count := _drain_events_to_log(30)
	_refresh_snapshot(event_count, "Done — AI run, %d events." % event_count)

# --- Interactive run ---

func _on_play_pressed() -> void:
	if _interactive_running:
		return
	_destroy_session()
	_interactive_running = true
	_stop_requested = false
	run_button.disabled = true
	play_button.disabled = true
	stop_button.disabled = false
	event_log.text = ""
	summary_label.text = ""
	status_label.text = "Starting interactive session…"
	await get_tree().process_frame

	_session = DuneSession.new()
	if not _session.create_interactive(int(seed_spin.value), int(players_spin.value)):
		status_label.text = "FAIL: create_interactive returned NULL"
		_end_interactive()
		return

	# Bootstrap: kick the engine off Idle into its first stable state.
	# After that, every loop iteration's state-advance happens inside
	# submit_decision (it returns the post-submit state code), so we must NOT
	# call step() again — that would be a step from AwaitingDecision and
	# return DUNE_ERR_STATE.
	var rc: int = _session.step()
	while true:
		await get_tree().process_frame
		if _stop_requested:
			status_label.text = "Stopped."
			break

		_drain_events_to_log(30)
		_refresh_snapshot(-1, "")

		if rc == DuneSession.RESULT_DONE:
			status_label.text = "Game complete."
			break
		if rc != DuneSession.RESULT_PENDING:
			status_label.text = "step rc=%d, stopping." % rc
			break

		# Pending decision — hand it to the panel and wait.
		var req_json := _session.get_pending_decision()
		var parsed: Variant = JSON.parse_string(req_json)
		if typeof(parsed) != TYPE_DICTIONARY:
			status_label.text = "decision JSON parse failed."
			break
		var req: Dictionary = parsed
		status_label.text = "Awaiting decision: %s" % str(req.get("kind", "?"))
		decision_panel.show_decision(req)
		var value: String = await decision_panel.submitted
		decision_panel.hide_decision()
		var sub := JSON.stringify({"value": value, "correlation_id": req.get("correlation_id", 0)})
		# submit_decision returns the next state code (PENDING/DONE/ERR).
		# Don't call step() afterwards — submit IS the step.
		rc = _session.submit_decision(sub)
		if rc < 0:
			status_label.text = "submit_decision rc=%d, stopping." % rc
			break

	_end_interactive()

func _on_stop_pressed() -> void:
	_stop_requested = true
	# Emit an empty submit if the panel is waiting, to unblock the await.
	if decision_panel.visible:
		decision_panel.submitted.emit("")
		decision_panel.hide_decision()

func _end_interactive() -> void:
	_interactive_running = false
	run_button.disabled = false
	play_button.disabled = false
	stop_button.disabled = true
	decision_panel.hide_decision()
	_destroy_session()

# --- Common ---

func _destroy_session() -> void:
	if _session != null:
		_session.destroy()
		_session = null

# Drain up to N most-recent events into the side panel. Returns the count
# drained this call.
func _drain_events_to_log(tail_limit: int) -> int:
	if _session == null:
		return 0
	var tail: Array[String] = []
	for line in event_log.text.split("\n", false):
		if not line.is_empty():
			tail.append(line)

	var added := 0
	while true:
		var ev_json := _session.poll_event()
		if ev_json.is_empty():
			break
		added += 1
		var summary := _summarise_event(ev_json)
		if not summary.is_empty():
			tail.append(summary)
			while tail.size() > tail_limit:
				tail.pop_front()
	event_log.text = "\n".join(tail)
	return added

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

# Refresh the map view + side panel from the current session's snapshot.
# Pass event_count >= 0 to update the "drained" line; pass extra_status to
# overwrite the status label.
func _refresh_snapshot(event_count: int, extra_status: String) -> void:
	if _session == null:
		return
	var snapshot_json := _session.get_snapshot()
	var parsed: Variant = JSON.parse_string(snapshot_json)
	if typeof(parsed) != TYPE_DICTIONARY:
		return
	var snapshot: Dictionary = parsed
	map_view.set_snapshot(snapshot)
	_render_summary(snapshot, event_count)
	if not extra_status.is_empty():
		status_label.text = extra_status

func _render_summary(snapshot: Dictionary, event_count: int) -> void:
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
	if event_count >= 0:
		lines.append("Events drained: %d" % event_count)
	lines.append("")
	lines.append("Spice:")
	for p in players:
		lines.append("  %-14s  %d" % [str(p.get("faction_name", "?")), int(p.get("spice", 0))])
	summary_label.text = "\n".join(lines)
