#!/usr/bin/env python3
"""Convert a treachery.online skin pack into a Dune Godot skin.

    python3 tools/skin_import.py godot/assets/skin-gf9art-zimmer.json
    python3 tools/skin_import.py --all            # every pack in godot/assets/

A treachery.online pack (`skin-*.json`) carries the official board geometry
(territory outlines as SVG paths, token/spice placement points, planet centre
and radius) plus URLs for the art. This script:

  1. Reads the engine's territory table from srcs/map.cpp (names + sectors).
  2. Matches pack territories to engine territories by name.
  3. Calibrates the 18 storm sectors against the board by fitting the angle
     of every single-sector territory, then checks every territory's outline
     against the engine's sector list and reports mismatches.
  4. Computes a force anchor for every engine (territory, sector) pair, using
     the pack's hand-placed point when one falls in that sector and the centre
     of the outline clipped to the sector wedge otherwise. Same for spice.
  5. Downloads the board image and faction token art into
     godot/assets/cache/<art>/ (shared between packs with the same art).
  6. Writes godot/assets/skins/<pack>.json in the format dune_skin.gd reads.

Everything it writes lives under godot/assets/, which is gitignored: the art
is copyrighted (GF9 / treachery.online) and stays on this machine. Only this
script is committed.

Standard library only.
"""
import argparse
import glob
import json
import math
import os
import re
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GODOT = os.path.join(REPO, "godot")
ASSETS = os.path.join(GODOT, "assets")
MAP_CPP = os.path.join(REPO, "srcs", "map.cpp")
TREACHERY_BASE = "https://treachery.online/"

SECTOR_COUNT = 18
SECTOR_DEG = 360.0 / SECTOR_COUNT

# Pack faction keys -> engine faction names (as they appear in snapshots).
FACTIONS = {
    "Green": "Atreides",
    "Black": "Harkonnen",
    "Yellow": "Fremen",
    "Red": "Emperor",
    "Orange": "Spacing Guild",
    "Blue": "Bene Gesserit",
}


# --- engine map -------------------------------------------------------------

def read_engine_territories(path):
    """name -> {sectors: [int], spice_sector: int|None} from srcs/map.cpp."""
    src = open(path, encoding="utf-8").read()
    out = {}
    pat = re.compile(
        r'addTerritory\("([^"]+)",\s*terrainType::\w+,\s*(\d+),\s*\{([^}]*)\}(?:,\s*(\d+))?')
    for m in pat.finditer(src):
        out[m.group(1)] = {
            "sectors": [int(x) for x in m.group(3).split(",") if x.strip()],
            "spice_sector": int(m.group(4)) if m.group(4) else None,
        }
    if len(out) < 40:
        sys.exit(f"error: parsed only {len(out)} territories from {path}")
    return out


def norm(name):
    return name.lower().replace("'", "").strip()


# --- geometry ---------------------------------------------------------------

def parse_svg_path(s):
    """Absolute M/L/C/Z subset -> list of polygons (lists of (x, y))."""
    toks = re.findall(r"[MLCZmlcz]|-?\d*\.?\d+(?:e-?\d+)?", s)
    polys, cur, cmd, i = [], [], None, 0
    while i < len(toks):
        t = toks[i]
        if t.isalpha():
            if t.islower() and t not in "z":
                raise ValueError(f"relative SVG command {t!r} not supported")
            cmd = t
            i += 1
            if cmd in "Zz" and cur:
                polys.append(cur)
                cur = []
            continue
        if cmd in ("M", "L"):
            if cmd == "M":
                if cur:
                    polys.append(cur)
                cur = []
                cmd = "L"  # implicit lineto after the first moveto pair
            cur.append((float(toks[i]), float(toks[i + 1])))
            i += 2
        elif cmd == "C":
            p0 = cur[-1]
            p1, p2, p3 = [(float(toks[i + k]), float(toks[i + k + 1])) for k in (0, 2, 4)]
            i += 6
            for k in range(1, 9):  # flatten the cubic into 8 segments
                u = k / 8.0
                a, b, c, d = (1 - u) ** 3, 3 * (1 - u) ** 2 * u, 3 * (1 - u) * u ** 2, u ** 3
                cur.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
                            a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
        else:
            raise ValueError(f"unexpected token {t!r} in SVG path")
    if cur:
        polys.append(cur)
    return polys


def area(poly):
    return 0.5 * sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]))


def centroid(poly):
    a = area(poly)
    if abs(a) < 1e-9:
        n = len(poly)
        return (sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n)
    cx = cy = 0.0
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        f = x1 * y2 - x2 * y1
        cx += (x1 + x2) * f
        cy += (y1 + y2) * f
    return (cx / (6 * a), cy / (6 * a))


def point_in(pt, poly):
    x, y = pt
    inside = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


def clip_convex(subject, clip):
    """Sutherland-Hodgman: clip `subject` by convex polygon `clip`."""
    def side(p, a, b):
        return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    orient = 1 if area(clip) > 0 else -1
    out = subject
    for a, b in zip(clip, clip[1:] + clip[:1]):
        if not out:
            break
        inp, out = out, []
        for p, q in zip(inp, inp[1:] + inp[:1]):
            pin, qin = side(p, a, b) * orient >= 0, side(q, a, b) * orient >= 0
            if pin:
                out.append(p)
            if pin != qin:
                dp, dq = side(p, a, b), side(q, a, b)
                t = dp / (dp - dq)
                out.append((p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])))
    return out


class Board:
    """Sector calibration: centre angle of sector s = base - 20*s (degrees,
    screen space, y down). Fitted from the pack, not hard-coded."""

    def __init__(self, cx, cy, radius):
        self.cx, self.cy, self.radius = cx, cy, radius
        self.base = 0.0

    def angle(self, p):
        return math.degrees(math.atan2(p[1] - self.cy, p[0] - self.cx))

    def sector_center_deg(self, s):
        return self.base - SECTOR_DEG * s

    def sector_of(self, p):
        a = self.angle(p)
        s = round((self.base - a) / SECTOR_DEG)
        return (s - 1) % SECTOR_COUNT + 1

    def wedge(self, s):
        c = math.radians(self.sector_center_deg(s))
        h = math.radians(SECTOR_DEG / 2)
        r = self.radius * 3
        return [(self.cx, self.cy),
                (self.cx + r * math.cos(c - h), self.cy + r * math.sin(c - h)),
                (self.cx + r * math.cos(c), self.cy + r * math.sin(c)),
                (self.cx + r * math.cos(c + h), self.cy + r * math.sin(c + h))]

    def fit(self, samples):
        """samples: [(point, sector)] -> sets base by circular mean."""
        vx = vy = 0.0
        for p, s in samples:
            b = math.radians(self.angle(p) + SECTOR_DEG * s)
            vx += math.cos(b)
            vy += math.sin(b)
        self.base = math.degrees(math.atan2(vy, vx))
        worst = max(abs(((self.angle(p) - self.sector_center_deg(s) + 180) % 360) - 180)
                    for p, s in samples)
        return worst


# --- downloads --------------------------------------------------------------

def css_color(v, default):
    """CSS colour ('rgba(255,100,100,0.5)', 'rgb(..)', '#hex', 'white') ->
    a string Godot's Color() parses ('#rrggbbaa' or a named colour)."""
    if not isinstance(v, str) or not v.strip():
        return default
    v = v.strip()
    m = re.fullmatch(r"rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)", v)
    if m:
        r, g, b = (max(0, min(255, round(float(x)))) for x in m.groups()[:3])
        a = max(0, min(255, round(float(m.group(4) or 1) * 255)))
        return f"#{r:02x}{g:02x}{b:02x}{a:02x}"
    return v


def resolve_url(u):
    if u.startswith("./"):
        return TREACHERY_BASE + u[2:]
    return u


def fetch(url, dest, offline):
    """Download url -> dest (skips if present). Returns res:// path or ''."""
    if not url:
        return ""
    if not os.path.exists(dest):
        if offline:
            return ""
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        req = urllib.request.Request(resolve_url(url), headers={"User-Agent": "dune-skin-import"})
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                data = r.read()
            if dest.endswith(".svg") and data.startswith(b"\xef\xbb\xbf"):
                data = data[3:]  # UTF-8 BOM trips Godot's SVG loader
            with open(dest + ".part", "wb") as f:
                f.write(data)
            os.replace(dest + ".part", dest)
        except Exception as e:  # noqa: BLE001 - report and carry on
            print(f"  warning: could not download {url}: {e}")
            return ""
    return "res://" + os.path.relpath(dest, GODOT).replace(os.sep, "/")


def art_slug(pack_path):
    # skin-gf9art-zimmer.json -> gf9art ; art is shared across music variants.
    m = re.match(r"skin-([a-z0-9]+)-", os.path.basename(pack_path))
    return m.group(1) if m else "misc"


def url_file(url):
    return os.path.basename(resolve_url(url).split("?")[0])


# --- conversion -------------------------------------------------------------

def convert(pack_path, engine, offline, verbose):
    pack = json.load(open(pack_path, encoding="utf-8"))
    slug = os.path.splitext(os.path.basename(pack_path))[0].removeprefix("skin-")
    cache = os.path.join(ASSETS, "cache", art_slug(pack_path))
    print(f"== {os.path.basename(pack_path)}  ({pack.get('Description', '')})")

    names = pack["TerritoryName_STR"]
    by_norm = {norm(v): k for k, v in names.items()}
    polys = {k: parse_svg_path(v) for k, v in pack["TerritoryBorder_SVG"].items()}

    # 1. engine name -> pack territory id
    tid = {}
    for name in engine:
        k = by_norm.get(norm(name))
        if k is None or k not in polys:
            sys.exit(f"error: engine territory {name!r} has no outline in the pack")
        tid[name] = k

    # 2. sector calibration from single-sector territories
    pc = pack["PlanetCenter"]
    board = Board(pc["X"], pc["Y"], pack["PlanetRadius"])
    samples = []
    for name, e in engine.items():
        if len(e["sectors"]) == 1:
            main = max(polys[tid[name]], key=lambda p: abs(area(p)))
            samples.append((centroid(main), e["sectors"][0]))
    worst = board.fit(samples)
    print(f"  sector calibration: sector s centred at {board.base:.1f} - 20*s deg "
          f"(worst single-sector residual {worst:.1f} deg)")

    # 3. which pack location points fall in which (territory, sector)
    def owner(pt):
        hits = [(abs(area(q)), k) for k, ps in polys.items() for q in ps if point_in(pt, q)]
        return min(hits)[1] if hits else None  # smallest containing outline wins

    loc_owner = {}
    for lk, p in pack["LocationCenter_Point"].items():
        pt = (p["X"], p["Y"])
        loc_owner[lk] = (owner(pt), board.sector_of(pt), pt)

    # 4. verify engine sectors against outline coverage, compute anchors
    territories = {}
    mismatches = 0
    for name, e in engine.items():
        k = tid[name]
        outline = polys[k]
        anchors = {}
        for s in e["sectors"]:
            if name == "Polar Sink":
                c = centroid(max(outline, key=lambda q: abs(area(q))))
                anchors[s] = {"force_anchor": {"x": round(c[0], 1), "y": round(c[1], 1)}}
                continue
            picked = [pt for (o, ls, pt) in loc_owner.values() if o == k and ls == s]
            if picked:
                pt = picked[0]
            else:
                pieces = [clip_convex(q, board.wedge(s)) for q in outline]
                pieces = [q for q in pieces if len(q) >= 3 and abs(area(q)) > 1.0]
                if not pieces:
                    mismatches += 1
                    print(f"  MISMATCH: {name} sector {s} - outline does not reach that sector")
                    continue
                pt = centroid(max(pieces, key=lambda q: abs(area(q))))
            anchors[s] = {"force_anchor": {"x": round(pt[0], 1), "y": round(pt[1], 1)}}
        # sectors the outline covers that the engine doesn't list (sanity)
        if name != "Polar Sink" and verbose:
            covered = set()
            for q in outline:
                for s in range(1, SECTOR_COUNT + 1):
                    piece = clip_convex(q, board.wedge(s))
                    if len(piece) >= 3 and abs(area(piece)) > 0.15 * abs(area(q)) / max(1, len(e["sectors"])):
                        covered.add(s)
            extra = covered - set(e["sectors"])
            if extra:
                print(f"  note: {name} outline also spans sectors {sorted(extra)} (engine: {e['sectors']})")
        territories[name] = {
            "border_polygons": [[round(v, 1) for xy in q for v in xy] for q in outline],
            "sectors": {str(s): a for s, a in anchors.items()},
        }

    # spice points: keyed by location id
    for lk, p in pack.get("LocationSpice_Point", {}).items():
        o, s, _ = loc_owner.get(lk, (None, None, None))
        name = next((n for n, k in tid.items() if k == o), None)
        if name is None:
            continue
        sec = engine[name]["spice_sector"] or s
        entry = territories[name]["sectors"].setdefault(str(sec), {})
        entry["spice_anchor"] = {"x": p["X"], "y": p["Y"]}
    for name, e in engine.items():
        if e["spice_sector"] and "spice_anchor" not in territories[name]["sectors"].get(str(e["spice_sector"]), {}):
            print(f"  note: no pack spice point for {name} (sector {e['spice_sector']})")

    # 5. art
    map_url = pack.get("Map_URL", "")
    map_file = os.path.join(cache, url_file(map_url)) if map_url else ""
    map_res = fetch(map_url, map_file, offline) if map_url else ""
    if not map_res:
        print("  warning: no board image - the skin will fall back to the procedural map")

    factions = {}
    for key, fname in FACTIONS.items():
        def art(field):
            u = pack.get(field, {}).get(key, "")
            return fetch(u, os.path.join(cache, url_file(u)), offline) if u else ""
        factions[fname] = {
            "color": css_color(pack["FactionColor"].get(key), "#646464"),
            "text_color": css_color(pack.get("FORCETOKEN_FONTCOLOR"), "#ffffff"),
            "force_token_image": art("FactionForceImage_URL"),
            "elite_force_token_image": art("FactionSpecialForceImage_URL"),
            "icon_image": art("FactionImage_URL"),
        }

    dims = pack["MapDimensions"]
    skin = {
        "schema_version": 1,
        "name": pack.get("Description", slug),
        "description": f"Imported from {os.path.basename(pack_path)} by tools/skin_import.py",
        "author": "treachery.online pack (local import, not redistributable)",
        "map": {
            "image": map_res,
            "width": dims["X"],
            "height": dims["Y"],
            "background_color": "#14161c",
        },
        "board": {
            "center": {"x": board.cx, "y": board.cy},
            "radius": board.radius,
            "sector_center_base_deg": round(board.base, 3),
            "sector_step_deg": -SECTOR_DEG,
            "storm_color": css_color(pack.get("STORM_OVERLAY_COLOR"), "#ff000055"),
            "highlight_color": css_color(pack.get("HIGHLIGHT_OVERLAY_COLOR"), "#ffffff44"),
            "token_radius": pack.get("FORCETOKEN_RADIUS", 11),
            "spice_color": css_color(pack.get("RESOURCETOKEN_COLOR"), "#ffd700"),
            "spice_radius": pack.get("RESOURCETOKEN_RADIUS", 8),
        },
        "territories": territories,
        "factions": factions,
    }
    out_dir = os.path.join(ASSETS, "skins")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, slug + ".json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(skin, f, indent=1)
    print(f"  wrote {os.path.relpath(out, REPO)}  ({mismatches} sector mismatches)")
    return mismatches


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("packs", nargs="*", help="treachery.online skin-*.json files")
    ap.add_argument("--all", action="store_true", help="import every godot/assets/skin-*.json")
    ap.add_argument("--offline", action="store_true", help="don't download; use cached art only")
    ap.add_argument("-v", "--verbose", action="store_true", help="also report extra outline coverage")
    args = ap.parse_args()
    packs = sorted(glob.glob(os.path.join(ASSETS, "skin-*.json"))) if args.all else args.packs
    if not packs:
        ap.error("give pack files or --all")
    engine = read_engine_territories(MAP_CPP)
    bad = sum(convert(p, engine, args.offline, args.verbose) for p in packs)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
