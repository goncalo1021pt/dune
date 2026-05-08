extends Control

# Smoke test: run the existing C smoke binary that links libdune.so and
# show the result inside Godot. Proves the toolchain works end-to-end:
# Godot can spawn processes, ffi_smoke can find libdune.so, and we can
# read its output back. This is NOT how Godot will eventually call into
# the engine — that path is GDExtension, landing in a follow-up PR.

@onready var status_label: Label   = $VBox/Status
@onready var run_button: Button    = $VBox/RunButton
@onready var output: RichTextLabel = $VBox/Output

func _ready() -> void:
	run_button.pressed.connect(_on_run_pressed)
	status_label.text = "Idle. Press the button to run tests/ffi_smoke."

func _on_run_pressed() -> void:
	# Linux-only smoke test: spawns the existing tests/ffi_smoke ELF binary
	# which links libdune.so. Windows-Godot can't run a Linux ELF, and we
	# don't cross-compile to libdune.dll / ffi_smoke.exe yet — that work
	# arrives with the GDExtension bridge. Until then, just bail politely.
	if OS.get_name() != "Linux":
		status_label.text = "Skipped: this OS.execute path is Linux-only."
		output.text = "OS.get_name() == %s\n\nThe libdune.so + ffi_smoke shell-out is a temporary toolchain check. The real Godot ↔ engine path is GDExtension, landing in the next PR — that will work on Linux + Windows + macOS." % OS.get_name()
		return

	status_label.text = "Running..."
	output.text = ""

	# Resolve repo root from res:// (the godot/ project sits one level under
	# the repo root). We need to:
	#   1. cd into the repo root so the binary's relative paths work
	#   2. set LD_LIBRARY_PATH=. so the dynamic linker finds libdune.so
	#      (libdune.so is at the repo root, not on a system path)
	var project_path := ProjectSettings.globalize_path("res://")
	var repo_root := project_path.get_base_dir()  # one level up from godot/

	var stdout: Array = []
	var exit_code := OS.execute(
		"/bin/bash",
		["-c", "cd '%s' && LD_LIBRARY_PATH=. ./tests/ffi_smoke" % repo_root],
		stdout,
		true,  # read_stderr — merge into stdout
	)

	# OS.execute populates `stdout` as an Array of String chunks; join them
	# back into one string for display.
	var joined := "\n".join(stdout)

	if exit_code == 0:
		status_label.text = "PASS (exit 0)"
		output.text = joined
	else:
		status_label.text = "FAIL (exit %d)" % exit_code
		output.text = joined
