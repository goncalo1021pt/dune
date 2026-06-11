extends PanelContainer

# Renders one pending DecisionRequest at a time.
#
# Engine emits exactly three kinds (see DecisionRequest in
# includes/headers/interaction/interaction_adapter.hpp):
#   "yn"     -> two buttons, Yes/No
#   "int"    -> SpinBox bounded by int_min..int_max, plus Submit
#   "select" -> one button per option, plus a Skip button if allow_none
#
# When the user picks something we build the matching response JSON
# (see decision_serialization.cpp::parseSubmitJson — the engine only
# requires {value: <string>, correlation_id?: <uint>}) and emit
# decision_submitted with it. The runner pipes it to
# DuneSession.submit_decision().

signal decision_submitted(response_json: String)

@onready var header: Label             = $VBox/Header
@onready var prompt_label: RichTextLabel = $VBox/Prompt
@onready var body: VBoxContainer       = $VBox/Body

var _correlation_id: int = 0


func _ready() -> void:
	clear()


# Clear the panel and hide it. Used between decisions and after a
# decision has been submitted so we don't leave stale widgets around.
func clear() -> void:
	header.text = ""
	prompt_label.text = ""
	for c in body.get_children():
		c.queue_free()
	visible = false


# Render a pending decision. `request` is the parsed JSON from
# DuneSession.get_pending_decision(). `actor_label` is a human-friendly
# string for the actor (e.g., "Atreides") — caller looks it up from the
# snapshot via actor_index.
func show_request(request: Dictionary, actor_label: String) -> void:
	clear()
	visible = true

	_correlation_id = int(request.get("correlation_id", 0))
	var kind: String = str(request.get("kind", ""))
	header.text = "Decision needed — %s" % actor_label
	prompt_label.text = str(request.get("prompt", ""))

	match kind:
		"yn":
			_build_yn()
		"int":
			var lo: int = int(request.get("int_min", 0))
			var hi: int = int(request.get("int_max", 0))
			_build_int(lo, hi)
		"select":
			var options: Array = request.get("options", [])
			var allow_none: bool = bool(request.get("allow_none", false))
			_build_select(options, allow_none)
		_:
			# Unknown kind — surface it instead of silently locking the UI.
			var label := Label.new()
			label.text = "Unsupported decision kind: %s" % kind
			body.add_child(label)


# --- Widget builders -------------------------------------------------

func _build_yn() -> void:
	var row := HBoxContainer.new()
	body.add_child(row)
	var yes := Button.new()
	yes.text = "Yes"
	yes.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	yes.pressed.connect(_submit.bind("y"))
	row.add_child(yes)
	var no := Button.new()
	no.text = "No"
	no.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	no.pressed.connect(_submit.bind("n"))
	row.add_child(no)


func _build_int(lo: int, hi: int) -> void:
	var spin := SpinBox.new()
	spin.min_value = lo
	spin.max_value = hi
	spin.step = 1
	spin.value = lo
	spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(spin)

	var submit := Button.new()
	submit.text = "Submit"
	submit.pressed.connect(func() -> void:
		_submit(str(int(spin.value)))
	)
	body.add_child(submit)


func _build_select(options: Array, allow_none: bool) -> void:
	# Options can be long when they're territory names; let the buttons
	# expand horizontally instead of clipping.
	for opt in options:
		var label_text: String = str(opt)
		var btn := Button.new()
		btn.text = label_text
		btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		btn.pressed.connect(_submit.bind(label_text))
		body.add_child(btn)
	if allow_none:
		var skip := Button.new()
		skip.text = "(skip)"
		skip.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		skip.pressed.connect(_submit.bind(""))
		body.add_child(skip)


# --- Submit ----------------------------------------------------------

func _submit(value: String) -> void:
	var payload := {
		"correlation_id": _correlation_id,
		"value": value,
	}
	# Hide the panel immediately so the user doesn't click twice — the
	# runner will call show_request() again if another decision pops.
	clear()
	decision_submitted.emit(JSON.stringify(payload))
