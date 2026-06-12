# Handoff — `feature/godot-interactive-and-skins`

Checkpoint left on 2026-06-12. Engine is green; the Godot feature work is a
**WIP checkpoint** — committed so it's backed up, not finished.

## Where things stand

- **Engine (C++):** healthy. `make shared` clean, `make tests` → 79/79 pass.
- **Branch:** `feature/godot-interactive-and-skins`, based on the `v1` line
  (which carries the Godot bridge, PRs #35/#36). **Note:** `main` is still at
  #34 (engine only) — the Godot work lives on `v1`, not `main`. Decide later
  whether `v1` eventually merges back to `main`.
- **This commit (WIP):** interactive step/submit loop + decision panel + skin
  system. Code-complete-ish but **never run live in Godot**.

## Open threads (pick up here)

1. **Verify the Godot run.** The interactive loop (`dune_runner.gd`
   step→get_pending_decision→submit) and `dune_decision_panel.gd` have not been
   executed in the editor. This is the #1 unknown — verify before building more.

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

3. **Stale native libs.** `godot/addons/dune/bin/libdune.so` is dated 2026-05-15;
   the root `libdune.so` was rebuilt 2026-05-30. If engine behavior changed,
   copy the fresh `libdune.so` over and/or rebuild the wrapper from
   `gdextension/` (`scons platform=linux target=template_debug`).
   `addons/dune/bin/` is gitignored (build artifacts), so this is local-only.

## Quick resume

```sh
make re && make tests          # confirm engine still green
make shared                    # rebuild libdune.so
# then open godot/ in Godot 4.5+, run scenes/main.tscn, hit Play (interactive)
```
