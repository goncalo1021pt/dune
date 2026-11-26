# Handoff

Last updated 2026-11-26. Engine is green and the Godot UI plays full games;
the board now renders with the official art via imported skin packs.

## Where things stand

- **Engine (C++):** healthy. `make shared` clean, `make tests` → 79/79 pass.
- **Branches:** `main` is the only long-lived branch and must stay working.
  Feature work goes on short-lived branches that merge by PR and are then
  deleted. The Godot interactive UI merged in #38, and this file's threads
  continue from there.

## Open threads (pick up here)

1. **Godot run — verified.** Verified headless
   on 2026-11-26: `make tests` 79/79, `ffi_smoke` + `ffi_smoke_interactive`
   PASS, `headless_test.gd` PASS (295 decisions), all scripts pass
   `--check-only`, and a driver that loads `main.tscn`, presses Play and clicks
   the first panel button each time played a full seed-42 / 6-player game to
   "Game complete." (295 decisions: 164 int, 130 select, 1 yn).
   **Still needs a human in the editor:** map rendering, layout, skin picker /
   image underlay, and whether the decisions are understandable to a player.

2. **Official board art — imported (2026-11-26).** `tools/skin_import.py`
   converts the treachery.online packs in `godot/assets/` (gitignored,
   copyrighted) into skins under `godot/assets/skins/`. It downloads art to
   `godot/assets/cache/` and checks the storm sectors against
   `srcs/map.cpp`. All 10 packs import with 0 sector mismatches. Board
   mode draws the real map, the storm on the real sector, faction tokens, and
   spice on the printed spice spots, and highlights territories on hover. See
   `godot/skins/README.md`. `Skins.zip` is still local-only, so back it up.
   **Next:** answer territory decisions by clicking the map (`DuneMapView`
   already emits `territory_clicked`), and show card and leader art.

3. **Native libs (local-only).** `addons/dune/bin/` is gitignored. Rebuilt on
   2026-11-26. After any engine change: `make shared`, then
   `cd gdextension && scons platform=linux target=template_debug` (it also
   copies `libdune.so` into `addons/dune/bin/`).

## Quick resume

```sh
make re && make tests          # confirm engine still green
make shared                    # rebuild libdune.so
godot --headless --path godot -s res://scripts/headless_test.gd   # bridge smoke test
# then open godot/ in Godot 4.5+, run scenes/main.tscn, hit Play (interactive)
```
