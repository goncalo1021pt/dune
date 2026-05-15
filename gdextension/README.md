# Dune GDExtension

Godot ↔ engine bridge. A thin C++ wrapper around `libdune.so`'s 12-symbol C
ABI exposed as the GDScript-callable `DuneSession` class.

## Layout

```
gdextension/
  SConstruct          build script (Linux + Windows cross-compile)
  src/
    register_types.{h,cpp}   GDExtension entry / class registration
    dune_session.{h,cpp}     RefCounted wrapper around the C ABI
godot-cpp/            git submodule, pinned to the 4.5 branch
godot/addons/dune/
  dune.gdextension    Godot manifest (which .so/.dll to load per platform)
  bin/                build output (gitignored)
```

## Build (Linux)

```bash
git submodule update --init --recursive
make shared                              # builds libdune.so
cd gdextension
scons platform=linux target=template_debug
```

The wrapper `.so` and a copy of `libdune.so` both land in
`godot/addons/dune/bin/`. Open `godot/project.godot` and the extension
loads automatically.

## Build (Windows, cross-compile from Linux)

```bash
sudo apt install mingw-w64               # one-time
make windows-shared                      # builds libdune.dll
cd gdextension
scons platform=windows target=template_debug use_mingw=yes
```

`g++-posix` is required (the engine uses `std::thread` / `std::condition_variable`,
which are stubs under the default win32 thread model). The Makefile already
points at `x86_64-w64-mingw32-g++-posix` for the engine DLL.

## What the wrapper exposes

`DuneSession` is a `RefCounted`. Drop the reference and the engine session
is destroyed (the destructor calls `dune_session_destroy`, which joins the
worker thread for interactive sessions).

| Method                                            | Mode    | Maps to                            |
|---------------------------------------------------|---------|------------------------------------|
| `DuneSession.api_version()` (static)              | both    | `dune_api_version`                 |
| `DuneSession.build_info()`  (static)              | both    | `dune_api_build_info`              |
| `create(seed, num_players)`                       | v1      | `dune_session_create`              |
| `create_interactive(seed, num_players)`           | v2      | `dune_session_create_interactive`  |
| `destroy()`                                       | both    | `dune_session_destroy`             |
| `is_open()` / `is_interactive()`                  | both    | wrapper bookkeeping                |
| `run_to_end()`                                    | v1      | `dune_session_run_to_end`          |
| `get_snapshot()`                                  | both    | `dune_session_get_snapshot`        |
| `poll_event()`                                    | both    | `dune_session_poll_event`          |
| `step()`                                          | v2      | `dune_session_step`                |
| `get_pending_decision()`                          | v2      | `dune_session_get_pending_decision`|
| `submit_decision(json)`                           | v2      | `dune_session_submit_decision`     |

`ResultCode` enum mirrors the `DUNE_*` return codes from `dune_c_api.h`
(`RESULT_OK`, `RESULT_DONE`, `RESULT_PENDING`, `RESULT_NO_EVENT`,
`RESULT_ERR_ARG`, `RESULT_ERR_STATE`, `RESULT_ERR_INTERNAL`).

The JSON shapes returned by `get_snapshot` / `poll_event` /
`get_pending_decision` are documented in `includes/Docs/snapshot_schema.md`.
