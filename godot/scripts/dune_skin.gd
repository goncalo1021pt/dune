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

# territory name -> { borders: Array[PackedVector2Array], sectors: { sector_int -> SectorEntry } }
# SectorEntry is { force_anchor: Vector2 or null, spice_anchor: Vector2 or null }
var territories: Dictionary = {}

# Board calibration (optional; written by tools/skin_import.py). Lets the
# renderer draw the storm over the real sectors of the map image.
# Centre angle of sector s (degrees, screen space, y down) is
#   sector_center_base_deg + sector_step_deg * s
var has_board: bool = false
var board_center: Vector2 = Vector2.ZERO
var board_radius: float = 0.0
var sector_center_base_deg: float = 0.0
var sector_step_deg: float = -20.0
var storm_color: Color = Color(1, 0, 0, 0.33)
var highlight_color: Color = Color(1, 1, 1, 0.27)
var token_radius: float = 11.0
var spice_color: Color = Color("#ffd700")
var spice_radius: float = 8.0

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

# Imported packs (res://assets/skins/, gitignored) come first so the picker
# defaults to real art when it is installed; committed skins follow.
const SKIN_DIRS := ["res://assets/skins/", "res://skins/"]

static func list_skin_files() -> Array[String]:
	var out: Array[String] = []
	for dir_path in SKIN_DIRS:
		var dir := DirAccess.open(dir_path)
		if dir == null:
			continue
		var found: Array[String] = []
		for f in dir.get_files():
			if f.ends_with(".json"):
				found.append(dir_path.path_join(f))
		found.sort()
		out.append_array(found)
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

	_parse_board(d.get("board", {}))
	_parse_territories(d.get("territories", {}))
	_parse_factions(d.get("factions", {}))

func _parse_board(b: Dictionary) -> void:
	if b.is_empty():
		return
	var c = _parse_point(b.get("center"))
	if c == null or float(b.get("radius", 0)) <= 0.0:
		push_warning("DuneSkin: board section missing center/radius, ignoring it")
		return
	has_board = true
	board_center = c
	board_radius = float(b["radius"])
	sector_center_base_deg = float(b.get("sector_center_base_deg", 0.0))
	sector_step_deg = float(b.get("sector_step_deg", -20.0))
	storm_color = _parse_color(b.get("storm_color"), storm_color)
	highlight_color = _parse_color(b.get("highlight_color"), highlight_color)
	token_radius = float(b.get("token_radius", token_radius))
	spice_color = _parse_color(b.get("spice_color"), spice_color)
	spice_radius = float(b.get("spice_radius", spice_radius))

func _parse_territories(td: Dictionary) -> void:
	for terr_name_v in td.keys():
		var terr_name := String(terr_name_v)
		var t: Dictionary = td[terr_name_v]
		var borders: Array[PackedVector2Array] = []
		# border_polygons: [[x0, y0, x1, y1, ...], ...] (importer output) —
		# preferred over border_svg, and supports multi-part territories.
		for flat in t.get("border_polygons", []):
			var poly := PackedVector2Array()
			var arr: Array = flat
			for i in range(0, arr.size() - 1, 2):
				poly.push_back(Vector2(float(arr[i]), float(arr[i + 1])))
			if poly.size() >= 3:
				borders.append(poly)
		if borders.is_empty():
			var svg_poly := _parse_svg_path(str(t.get("border_svg", "")))
			if svg_poly.size() >= 3:
				borders.append(svg_poly)
		var entry := {
			"borders": borders,
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
	return not map_image_path.is_empty() and get_map_texture() != null

func get_map_texture() -> Texture2D:
	return _load_texture(map_image_path, MAP_SVG_RASTER_PX)

# Returns the force anchor for (territory, sector) in skin pixel space, or
# null if the skin doesn't specify one. An unknown sector (e.g. 0 before the
# first storm) falls back to the territory's lowest listed sector.
func force_anchor(territory_name: String, sector: int) -> Variant:
	var s = _sector_entry(territory_name, sector)
	if s == null: return null
	return s.force_anchor

# Each territory has a single printed spice spot on the board, but the engine
# may report the pile in another of the territory's sectors, so fall back to
# whichever sector carries the spice anchor.
func spice_anchor(territory_name: String, sector: int) -> Variant:
	var s = _sector_entry(territory_name, sector)
	if s != null and s.spice_anchor != null:
		return s.spice_anchor
	var t = territories.get(territory_name)
	if t == null: return null
	for k in t.sectors:
		if t.sectors[k].spice_anchor != null:
			return t.sectors[k].spice_anchor
	return null

func _sector_entry(territory_name: String, sector: int) -> Variant:
	var t = territories.get(territory_name)
	if t == null or t.sectors.is_empty(): return null
	if t.sectors.has(sector):
		return t.sectors[sector]
	var keys: Array = t.sectors.keys()
	keys.sort()
	return t.sectors[keys[0]]

# Border polygons for a territory in skin pixel space (may be several).
func territory_borders(territory_name: String) -> Array:
	var t = territories.get(territory_name)
	if t == null: return []
	return t.borders

# Name of the territory under `p` (skin pixel space), or "" if none. When
# outlines nest (a sietch inside its surrounding flat) the smallest wins.
func territory_at(p: Vector2) -> String:
	var best := ""
	var best_area := INF
	for terr_name in territories:
		for poly in territories[terr_name].borders:
			if Geometry2D.is_point_in_polygon(p, poly):
				var a := absf(_poly_area(poly))
				if a < best_area:
					best_area = a
					best = terr_name
	return best

# Centre angle (radians) of storm sector `s`, if the skin has a board.
func sector_center_angle(s: int) -> float:
	return deg_to_rad(sector_center_base_deg + sector_step_deg * float(s))

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

# Imported resources load through the ResourceLoader. Files nobody has
# imported yet (freshly downloaded art under res://assets/, run headless or
# before the editor rescans) are decoded straight from disk instead.
func _load_texture(path: String, svg_raster_px: float = SVG_RASTER_PX) -> Texture2D:
	if path.is_empty(): return null
	if _texture_cache.has(path):
		return _texture_cache[path]
	var tex: Texture2D = null
	if ResourceLoader.exists(path):
		tex = load(path) as Texture2D
	elif FileAccess.file_exists(path):
		tex = _load_texture_from_disk(path, svg_raster_px)
	_texture_cache[path] = tex
	return tex

const SVG_RASTER_PX := 128.0       # SVG tokens are rasterised to this width
const MAP_SVG_RASTER_PX := 3000.0  # SVG boards get enough pixels to zoom

static func _load_texture_from_disk(path: String, svg_raster_px: float) -> Texture2D:
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.is_empty():
		return null
	var img := Image.new()
	var err := OK
	match path.get_extension().to_lower():
		"svg":
			var text := bytes.get_string_from_utf8()
			var m := RegEx.create_from_string("viewBox=\"[^\"]*?([\\d.]+)[ ,]+([\\d.]+)\"").search(text)
			var w := float(m.get_string(1)) if m else svg_raster_px
			err = img.load_svg_from_buffer(bytes, svg_raster_px / maxf(w, 1.0))
		"png":
			err = img.load_png_from_buffer(bytes)
		"jpg", "jpeg":
			err = img.load_jpg_from_buffer(bytes)
		"webp":
			err = img.load_webp_from_buffer(bytes)
		_:
			return null
	if err != OK:
		push_warning("DuneSkin: could not decode %s (err %d)" % [path, err])
		return null
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)

static func _poly_area(poly: PackedVector2Array) -> float:
	var a := 0.0
	for i in poly.size():
		var p := poly[i]
		var q := poly[(i + 1) % poly.size()]
		a += p.x * q.y - q.x * p.y
	return a * 0.5

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
	# from_string returns the fallback (instead of black) on unparseable
	# input such as CSS rgba() strings from an un-converted pack.
	return Color.from_string(s, fallback)
