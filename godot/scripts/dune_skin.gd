extends RefCounted
class_name DuneSkin

# Typed accessor for a skin JSON file. See godot/skins/README.md for the
# schema. The loader is permissive — anything that fails to parse drops
# back to its default and emits a warning; missing optional fields are
# treated as "not provided" so the renderer falls back to procedural.

const SCHEMA_VERSION_SUPPORTED := 1

var schema_version: int = 0
var name: String = ""
var description: String = ""
var author: String = ""

# Map background.
var map_image_path: String = ""
var map_size: Vector2 = Vector2.ZERO
var background_color: Color = Color("#1a1a1a")

# territory name -> { border: PackedVector2Array, sectors: { sector_int -> SectorEntry } }
# SectorEntry is { force_anchor: Vector2 or null, spice_anchor: Vector2 or null }
var territories: Dictionary = {}

# faction name -> { color, text_color, force_token_image, elite_force_token_image, icon_image }
var factions: Dictionary = {}

# Loaded textures, keyed by the original path so repeated lookups are cheap.
var _texture_cache: Dictionary = {}

# --- Loading ---

# Returns a DuneSkin or null. Typed as RefCounted (not DuneSkin) so the
# script compiles on the first parse before its own class_name has been
# registered globally — without this, every reference to DuneSkin in other
# files fails until Godot's class table has been seeded by a second pass.
static func load_from_file(path: String) -> RefCounted:
	if not FileAccess.file_exists(path):
		push_warning("DuneSkin: file not found at %s" % path)
		return null
	var text := FileAccess.get_file_as_string(path)
	if text.is_empty():
		push_warning("DuneSkin: empty file at %s" % path)
		return null
	var parsed: Variant = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		push_warning("DuneSkin: JSON did not parse as a Dictionary (%s)" % path)
		return null
	# Self-instantiate via the script file rather than the class_name so the
	# first-parse pass doesn't trip on its own forward reference.
	var skin = load("res://scripts/dune_skin.gd").new()
	skin._parse(parsed as Dictionary)
	return skin

static func list_skin_files(dir_path: String = "res://skins/") -> Array[String]:
	var out: Array[String] = []
	var dir := DirAccess.open(dir_path)
	if dir == null:
		return out
	for f in dir.get_files():
		if f.ends_with(".json"):
			out.append(dir_path.path_join(f))
	out.sort()
	return out

func _parse(d: Dictionary) -> void:
	schema_version = int(d.get("schema_version", 0))
	if schema_version != SCHEMA_VERSION_SUPPORTED:
		push_warning(
			"DuneSkin: schema_version=%d but renderer expects %d. Continuing best-effort." % [
				schema_version, SCHEMA_VERSION_SUPPORTED
			]
		)
	name = str(d.get("name", "Unnamed"))
	description = str(d.get("description", ""))
	author = str(d.get("author", ""))

	var m: Dictionary = d.get("map", {})
	map_image_path = str(m.get("image", ""))
	map_size = Vector2(float(m.get("width", 0)), float(m.get("height", 0)))
	var bg := str(m.get("background_color", "")).strip_edges()
	if not bg.is_empty():
		background_color = Color(bg)

	_parse_territories(d.get("territories", {}))
	_parse_factions(d.get("factions", {}))

func _parse_territories(td: Dictionary) -> void:
	for terr_name_v in td.keys():
		var terr_name := String(terr_name_v)
		var t: Dictionary = td[terr_name_v]
		var entry := {
			"border": _parse_svg_path(str(t.get("border_svg", ""))),
			"sectors": {},
		}
		var sectors: Dictionary = t.get("sectors", {})
		for sector_key_v in sectors.keys():
			var sector_idx := int(String(sector_key_v))
			var s: Dictionary = sectors[sector_key_v]
			entry.sectors[sector_idx] = {
				"force_anchor": _parse_point(s.get("force_anchor")),
				"spice_anchor": _parse_point(s.get("spice_anchor")),
			}
		territories[terr_name] = entry

func _parse_factions(fd: Dictionary) -> void:
	for fac_name_v in fd.keys():
		var fac_name := String(fac_name_v)
		var f: Dictionary = fd[fac_name_v]
		factions[fac_name] = {
			"color":                   _parse_color(f.get("color"), Color.GRAY),
			"text_color":              _parse_color(f.get("text_color"), Color.WHITE),
			"force_token_image":       str(f.get("force_token_image", "")),
			"elite_force_token_image": str(f.get("elite_force_token_image", "")),
			"icon_image":              str(f.get("icon_image", "")),
		}

# --- Accessors ---

func has_map_image() -> bool:
	return not map_image_path.is_empty() and ResourceLoader.exists(map_image_path)

func get_map_texture() -> Texture2D:
	return _load_texture(map_image_path)

# Returns the force anchor for (territory, sector) in skin pixel space, or
# null if the skin doesn't specify one. Callers fall back to procedural.
func force_anchor(territory_name: String, sector: int) -> Variant:
	var t = territories.get(territory_name)
	if t == null: return null
	var s = t.sectors.get(sector)
	if s == null: return null
	return s.force_anchor

func spice_anchor(territory_name: String, sector: int) -> Variant:
	var t = territories.get(territory_name)
	if t == null: return null
	var s = t.sectors.get(sector)
	if s == null: return null
	return s.spice_anchor

# PackedVector2Array of border points in skin pixel space, or empty.
func territory_border(territory_name: String) -> PackedVector2Array:
	var t = territories.get(territory_name)
	if t == null: return PackedVector2Array()
	return t.border

func faction_color(faction_name: String, fallback: Color) -> Color:
	var f = factions.get(faction_name)
	if f == null: return fallback
	return f.color

func faction_text_color(faction_name: String, fallback: Color) -> Color:
	var f = factions.get(faction_name)
	if f == null: return fallback
	return f.text_color

# Returns the loaded force-token texture for a faction or null if the skin
# doesn't ship one. Use `elite` to request the elite variant; if the elite
# variant isn't specified, falls back to the normal one.
func faction_force_texture(faction_name: String, elite: bool = false) -> Texture2D:
	var f = factions.get(faction_name)
	if f == null: return null
	var path: String = f.elite_force_token_image if elite else f.force_token_image
	if path.is_empty() and elite:
		path = f.force_token_image
	return _load_texture(path)

# --- Internals ---

func _load_texture(path: String) -> Texture2D:
	if path.is_empty(): return null
	if _texture_cache.has(path):
		return _texture_cache[path]
	if not ResourceLoader.exists(path):
		_texture_cache[path] = null
		return null
	var tex: Texture2D = load(path) as Texture2D
	_texture_cache[path] = tex
	return tex

# Parse the M/L/Z subset of SVG path syntax. Single-letter commands followed
# by space-separated coordinate pairs ("M243.4 297.6 L236.2 311.3 Z"). Curves
# and relative commands aren't supported because board outlines don't need
# them; if the skin uses them, the polygon will be coarse but valid.
func _parse_svg_path(svg: String) -> PackedVector2Array:
	var pts := PackedVector2Array()
	var text := svg.strip_edges()
	if text.is_empty(): return pts

	# Tokenize: split on whitespace, then route command letters vs numbers.
	var raw := text.replace(",", " ").split(" ", false)
	var i := 0
	while i < raw.size():
		var tok: String = raw[i]
		if tok.is_empty():
			i += 1
			continue
		var head := tok.substr(0, 1)
		if head == "M" or head == "L" or head == "m" or head == "l":
			# Coord might be glued to the command letter ("M243.4") or be
			# the next token. Handle both.
			var x_str := tok.substr(1) if tok.length() > 1 else ""
			if x_str.is_empty():
				i += 1
				if i >= raw.size(): break
				x_str = raw[i]
			i += 1
			if i >= raw.size(): break
			var y_str: String = raw[i]
			i += 1
			pts.push_back(Vector2(float(x_str), float(y_str)))
		elif head == "Z" or head == "z":
			i += 1
		else:
			# Bare coordinate pair (after a previous M/L). Treat as L.
			if i + 1 < raw.size():
				pts.push_back(Vector2(float(tok), float(raw[i + 1])))
				i += 2
			else:
				i += 1
	return pts

func _parse_point(v: Variant) -> Variant:
	if typeof(v) != TYPE_DICTIONARY:
		return null
	var d: Dictionary = v
	if not d.has("x") or not d.has("y"):
		return null
	return Vector2(float(d["x"]), float(d["y"]))

func _parse_color(v: Variant, fallback: Color) -> Color:
	if typeof(v) != TYPE_STRING:
		return fallback
	var s := String(v).strip_edges()
	if s.is_empty():
		return fallback
	return Color(s)
