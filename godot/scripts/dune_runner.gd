extends Control

# Drives a DuneSession and hands its snapshots to the map view + side panel.
#
# AI-vs-AI mode only for now: click Run, the engine runs to completion
# (synchronous), and the final snapshot + last events are rendered.
# Interactive mode (v2 ABI) is wired into DuneSession but needs a
# decision-prompt overlay before it can be exposed in the UI.

@onready var status_label: Label    = $HBox/Side/Status
@onready var seed_spin: SpinBox     = $HBox/Side/Controls/SeedSpin
@onready var players_spin: SpinBox  = $HBox/Side/Controls/PlayersSpin
@onready var run_button: Button     = $HBox/Side/Buttons/RunButton
@onready var event_log: RichTextLabel = $HBox/Side/EventLog
@onready var summary_label: Label   = $HBox/Side/Summary
@onready var map_view: DuneMapView  = $HBox/Map

var _session: DuneSession = null

func _ready() -> void:
	run_button.pressed.connect(_on_run_pressed)
	seed_spin.value = 42
	players_spin.value = 6
	status_label.text = "libdune API %s" % DuneSession.api_version()

func _on_run_pressed() -> void:
	status_label.text = "Running…"
	event_log.text = ""
	summary_label.text = ""
	# Defer one frame so the UI repaints "Running…" before the engine
	# blocks the main thread inside run_to_end().
	await get_tree().process_frame
	_run_session()

func _run_session() -> void:
	_session = DuneSession.new()
	if not _session.create(int(seed_spin.value), int(players_spin.value)):
		status_label.text = "FAIL: dune_session_create returned NULL"
		return

	var rc: int = _session.run_to_end()
	if rc != DuneSession.RESULT_DONE:
		status_label.text = "FAIL: run_to_end rc=%d" % rc
		return

	# Drain the event queue. Keep the last N for display so we don't OOM
	# the RichTextLabel — a full game emits a few hundred events.
	var event_count := 0
	var tail: Array[String] = []
	const TAIL_LIMIT := 30
	while true:
		var ev_json := _session.poll_event()
		if ev_json.is_empty():
			break
		event_count += 1
		var summary := _summarise_event(ev_json)
		if not summary.is_empty():
			tail.append(summary)
			if tail.size() > TAIL_LIMIT:
				tail.pop_front()

	# Final snapshot drives the map render.
	var snapshot_json := _session.get_snapshot()
	var snapshot: Variant = JSON.parse_string(snapshot_json)
	if typeof(snapshot) != TYPE_DICTIONARY:
		status_label.text = "FAIL: snapshot JSON didn't parse as a Dictionary"
		return
	map_view.set_snapshot(snapshot as Dictionary)
	_render_summary(snapshot as Dictionary, event_count)
	event_log.text = "\n".join(tail)
	status_label.text = "Done — drained %d events." % event_count

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
	lines.append("Events drained: %d" % event_count)
	lines.append("")
	lines.append("Spice:")
	for p in players:
		lines.append("  %-14s  %d" % [str(p.get("faction_name", "?")), int(p.get("spice", 0))])
	summary_label.text = "\n".join(lines)
