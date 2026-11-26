# Dune skins

A "skin" is a JSON file that tells the Godot frontend how to render the
engine state. Everything is optional except `schema_version` and the
top-level `name` — a skin can ship just the map image, or just a faction
palette, or the full set of card / leader / audio assets.

`template_skin.json` is the reference layout. Copy it, fill in the asset
paths under the keys that matter to you, drop the file in this directory,
and the editor's skin selector will pick it up.

## Importing the official board (treachery.online packs)

The usual way to get real art is to import a treachery.online skin pack
(`skin-*.json`) rather than author a skin by hand. The packs carry the
official board geometry (territory outlines, token and spice positions, planet
centre) and URLs for the art.

```sh
# packs live in godot/assets/ (gitignored)
python3 tools/skin_import.py --all                                # every pack
python3 tools/skin_import.py godot/assets/skin-gf9art-zimmer.json # just one
```

The importer downloads the board image and faction token art into
`godot/assets/cache/<art>/`, and writes converted skins to
`godot/assets/skins/`. The skin picker lists those first and remembers the
last one you chose. It also checks the board's 18 storm sectors against the
engine's sector table in `srcs/map.cpp`, and exits non-zero on any mismatch.

**Everything it writes stays in `godot/assets/`, which is gitignored.** The
art is copyrighted (GF9 / treachery.online). Only the importer is committed,
so never move imported files into a tracked directory, and don't distribute a
build that includes them.

Imported skins use three additions to the schema below:
`territories.<name>.border_polygons` (flat `[x0, y0, x1, y1, ...]` arrays,
several per territory allowed, preferred over `border_svg`), and a `board`
section:

```jsonc
"board": {
  "center": { "x": 281, "y": 311 },   // planet centre, map pixel space
  "radius": 242,
  "sector_center_base_deg": 129.8,    // centre angle of sector s is
  "sector_step_deg": -20,             //   base + step * s  (screen space, y down)
  "storm_color": "#ff646480",
  "highlight_color": "#ffffff80",
  "token_radius": 8, "spice_color": "#ff8c3ce6", "spice_radius": 8
}
```

## Schema

```jsonc
{
  "schema_version": 1,           // required; current version
  "name":           "<string>",  // required; shown in the picker
  "description":    "<string>",  // free-form
  "author":         "<string>",

  "map": {
    "image":            "res://<path>",   // background image; png/jpg/webp
    "width":            <int>,            // image pixel width
    "height":           <int>,            // image pixel height
    "background_color": "#<hex>"          // shown around the map image
  },

  // Engine territory name → (border polygon, per-sector anchors).
  // The renderer falls back to its procedural polar layout when this
  // section is missing or a specific territory is absent. Coordinates
  // are in the same pixel space as map.image.
  "territories": {
    "Arrakeen": {
      "border_svg": "M x y L x y L x y Z",  // M/L/Z subset only
      "sectors": {
        "10": {                              // sector index as a string
          "force_anchor": { "x": <int>, "y": <int> },
          "spice_anchor": { "x": <int>, "y": <int> }   // optional
        }
      }
    }
  },

  // Engine faction name → display.
  "factions": {
    "Atreides": {
      "color":                   "#<hex>",
      "text_color":              "#<hex>",   // text on top of color
      "force_token_image":       "res://<path>",   // normal forces
      "elite_force_token_image": "res://<path>",   // optional
      "icon_image":              "res://<path>"    // faction sigil
    }
  },

  // Future sections — present in the template for shape, not used yet.
  "treachery_cards": { "<card name>": { "image": "res://<path>" } },
  "leaders":         { "<leader name>": { "portrait": "res://<path>" } },
  "audio": {
    "phase_music": { "<PHASE>": "res://<path>" },
    "ui_sounds":   { "<event>": "res://<path>" }
  }
}
```

## Territory names

Engine canonical names (the strings emitted by the snapshot). All 42 are
in `template_skin.json` so you can fill them in without a typo:

```
Arrakeen        Carthag         Sietch Tabr     Habbanya Sietch
Tuek's Sietch   Polar Sink      Imperial Basin  Old Gap
Tsimpo          Hagga Basin     Plastic Basin   Sihaya Ridge
Hole in the Rock  Basin         Broken Land     Arsunt
Rock Outcroppings  Shield Wall  False Wall East False Wall South
False Wall West Pasty Mesa      The Minor Erg   Red Chasm
Gara Kulon      Rim Wall West   Bight of the Cliff
Cielago West    Cielago Depression  Cielago North  Cielago South
Cielago East    Meridian        Harg Pass       South Mesa
Wind Pass       Wind Pass North Habbanya Erg    Habbanya Ridge Flat
The Great Flat  Funeral Plain   The Greater Flat
```

## Faction names

```
Atreides   Harkonnen   Fremen   Emperor   Spacing Guild   Bene Gesserit
```

## Fallback behaviour

If a skin omits a section the renderer needs, it falls back to its
procedural defaults:
- No `map.image` → 18-sector polar wheel drawn from `dune_map_data.gd`.
- No `board` section → no storm overlay on the map image.
- No territory entry → procedural ring assignment used.
- No faction color → palette from `dune_map_data.gd` used.

This means a partially-filled skin still works; you can author iteratively.
