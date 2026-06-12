extends PanelContainer
class_name DuneDecisionPanel

# Renders a pending DecisionRequest from the FFI v2 protocol and waits for
# the user to pick a value. Emits `submitted(value: String)` with the string
# that should be wrapped in `{"value": ...}` and handed to
# dune_session_submit_decision.
#
# The three primitive kinds (per snapshot_schema.md §"Decision protocol") map
# to three concrete widget rows:
#   yn     → Yes / No buttons        → emits "y" / "n"
#   int    → SpinBox + Submit        → emits the decimal value as a string
#   select → one button per option   → emits the chosen option, or "" if
#                                       allow_none and the player skips
#
# We build the children in code so the scene file stays small. Style hooks
# (theme, custom_minimum_size etc.) can be applied from main.tscn.

signal submitted(value: String)

const PROMPT_FONT_SIZE := 12

@onready var _vbox: VBoxContainer = VBoxContainer.new()
@onready var _prompt: Label = Label.new()
@onready var _actor: Label = Label.new()
@onready var _yn_row: HBoxContainer = HBoxContainer.new()
@onready var _int_row: HBoxContainer = HBoxContainer.new()
@onready var _int_spin: SpinBox = SpinBox.new()
@onready var _int_submit: Button = Button.new()
@onready var _select_row: VBoxContainer = VBoxContainer.new()
@onready var _select_grid: VBoxContainer = VBoxContainer.new()
@onready var _select_skip: Button = Button.new()

var _kind: String = ""

func _ready() -> void:
	visible = false
	add_child(_vbox)
	_vbox.add_theme_constant_override("separation", 6)

	_actor.add_theme_color_override("font_color", Color(0.7, 0.85, 1.0))
	_vbox.add_child(_actor)

	_prompt.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_prompt.add_theme_font_size_override("font_size", PROMPT_FONT_SIZE)
	_vbox.add_child(_prompt)

	# yn row
	var yes := Button.new()
	yes.text = "Yes"
	yes.pressed.connect(func(): _emit("y"))
	var no := Button.new()
	no.text = "No"
	no.pressed.connect(func(): _emit("n"))
	_yn_row.add_child(yes)
	_yn_row.add_child(no)
	_vbox.add_child(_yn_row)

	# int row
	_int_spin.step = 1.0
	_int_spin.custom_minimum_size = Vector2(120, 0)
	_int_submit.text = "Submit"
	_int_submit.pressed.connect(func(): _emit(str(int(_int_spin.value))))
	_int_row.add_child(_int_spin)
	_int_row.add_child(_int_submit)
	_vbox.add_child(_int_row)

	# select row (header + option list + skip)
	_select_row.add_theme_constant_override("separation", 4)
	_select_row.add_child(_select_grid)
	_select_skip.text = "Skip"
	_select_skip.pressed.connect(func(): _emit(""))
	_select_row.add_child(_select_skip)
	_vbox.add_child(_select_row)

	_yn_row.visible = false
	_int_row.visible = false
	_select_row.visible = false

# Pass the parsed DecisionRequest JSON dictionary.
func show_decision(req: Dictionary) -> void:
	_kind = str(req.get("kind", ""))
	var actor_idx := int(req.get("actor_index", -1))
	_actor.text = "" if actor_idx < 0 else "Decision for player %d" % actor_idx
	_prompt.text = str(req.get("prompt", "")) if _kind != "" else "(no pending decision)"

	_yn_row.visible = (_kind == "yn")
	_int_row.visible = (_kind == "int")
	_select_row.visible = (_kind == "select")

	if _kind == "int":
		var lo := int(req.get("int_min", 0))
		var hi := int(req.get("int_max", 0))
		_int_spin.min_value = lo
		_int_spin.max_value = hi
		_int_spin.value = lo
		_int_submit.disabled = (hi < lo)
	elif _kind == "select":
		_clear_select_grid()
		var options: Array = req.get("options", [])
		for opt_v in options:
			var opt := String(opt_v)
			var btn := Button.new()
			btn.text = opt if not opt.is_empty() else "(empty)"
			btn.pressed.connect(func(): _emit(opt))
			_select_grid.add_child(btn)
		_select_skip.visible = bool(req.get("allow_none", false))
	visible = true

func hide_decision() -> void:
	visible = false
	_kind = ""

func _clear_select_grid() -> void:
	for c in _select_grid.get_children():
		c.queue_free()

func _emit(value: String) -> void:
	# Hide before emitting — the listener typically calls show_decision again
	# for the next decision, so we don't want a stale frame in between.
	visible = false
	emitted_value_cache = value
	submitted.emit(value)

# Last value emitted — useful for tests/debug, otherwise ignore.
var emitted_value_cache: String = ""
