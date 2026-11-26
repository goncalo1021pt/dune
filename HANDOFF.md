# Handoff — `godot-ui-verify`

Checkpoint left on 2026-06-12. Engine is green; the Godot feature work is a
**WIP checkpoint** — committed so it's backed up, not finished.

## Where things stand

- **Engine (C++):** healthy. `make shared` clean, `make tests` → 79/79 pass.
- **Branch:** `godot-ui-verify`, a short-lived branch off `main`. As of
  2026-11-26 `main` is the only long-lived branch (the old `v1` line was
  fast-forwarded into it and deleted). This branch merges into `main` once the
  Godot run below is verified. It supersedes PR #37's `decision_panel.gd`
  with `dune_decision_panel.gd`; #37's `headless_test.gd` and the Windows
  `DUNE_API` export fix are kept.
- **This branch:** interactive step/submit loop + decision panel + skin system.
  Logic verified headless on 2026-11-26 (Godot 4.6.2), see thread 1.

## Open threads (pick up here)

1. **Verify the Godot run — logic done, visuals pending.** Verified headless
   on 2026-11-26: `make tests` 79/79, `ffi_smoke` + `ffi_smoke_interactive`
   PASS, `headless_test.gd` PASS (295 decisions), all scripts pass
   `--check-only`, and a driver that loads `main.tscn`, presses Play and clicks
   the first panel button each time played a full seed-42 / 6-player game to
   "Game complete." (295 decisions: 164 int, 130 select, 1 yn).
   **Still needs a human in the editor:** map rendering, layout, skin picker /
   image underlay, and whether the decisions are understandable to a player.

2. **Real art skins are intentionally out of git.** `godot/assets/` is
   `.gitignore`d (line 17). The 10 skins there are **GF9 / treachery.online
   art (copyrighted)** — keep them out of the repo. They are NOT loadable as-is:
   they use a different schema (`TreacheryCardType_STR`, `DrawResourceIconsOnMap`)
   than `godot/skins/template_skin.json`, which is the format `dune_skin.gd`
   reads. To use them you'd need a **schema-conversion step**.
   - `godot/assets/Skins.zip` is the single bundle of all 10 → **back it up
     off-machine** (it's local-only since assets/ is ignored; also
     re-downloadable from treachery.online).
   - The skin picker scans `res://skins/`, which only holds `template_skin.json`.

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
