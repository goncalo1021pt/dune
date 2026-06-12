extends SceneTree

# Headless smoke test for the interactive ABI path.
#
# Creates an interactive DuneSession and drives it like the UI would, but
# always picks a sensible default (first option for select, int_min for
# int, "y" for yn). Asserts: every step returns PENDING until the engine
# reaches DONE; we drained at least one decision and a non-trivial event
# count. Used to verify the runner contract end-to-end without launching
# the editor.

func _init() -> void:
	var session := DuneSession.new()
	if not session.create_interactive(42, 6):
		printerr("FAIL: create_interactive returned false")
		quit(1)
		return

	var decisions: int = 0
	var events: int = 0
	var rc: int = session.step()
	while rc == DuneSession.RESULT_PENDING:
		decisions += 1
		if decisions > 10000:
			printerr("FAIL: exceeded 10000 decisions, likely infinite loop")
			quit(1)
			return

		# Drain events so the queue doesn't grow unbounded.
		while true:
			var ev := session.poll_event()
			if ev.is_empty():
				break
			events += 1

		var req_json := session.get_pending_decision()
		var parsed: Variant = JSON.parse_string(req_json)
		if typeof(parsed) != TYPE_DICTIONARY:
			printerr("FAIL: pending decision didn't parse: %s" % req_json)
			quit(1)
			return
		var req: Dictionary = parsed
		var value: String = _default_value(req)
		var resp := JSON.stringify({
			"correlation_id": int(req.get("correlation_id", 0)),
			"value": value,
		})
		rc = session.submit_decision(resp)

	# Final drain.
	while true:
		var ev := session.poll_event()
		if ev.is_empty():
			break
		events += 1

	if rc != DuneSession.RESULT_DONE:
		printerr("FAIL: terminal rc=%d (expected DONE=%d)" % [rc, DuneSession.RESULT_DONE])
		quit(1)
		return

	print("PASS: decisions=%d events=%d" % [decisions, events])
	quit(0)


func _default_value(req: Dictionary) -> String:
	var kind: String = str(req.get("kind", ""))
	match kind:
		"yn":
			return "y"
		"int":
			return str(int(req.get("int_min", 0)))
		"select":
			var options: Array = req.get("options", [])
			if options.is_empty():
				return ""
			return str(options[0])
		_:
			return ""
