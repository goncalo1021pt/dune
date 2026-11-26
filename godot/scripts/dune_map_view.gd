extends Control
class_name DuneMapView

# Map renderer for an Arrakis snapshot. Two layout modes:
#
#   1. Procedural (default): 18 storm sectors × 5 rings (centre = Polar Sink).
#      Each territory claims a (ring, sector-set) intersection drawn as an
#      annular sector polygon. No external assets required.
#
#   2. Skin-driven: if a DuneSkin with a map image is provided via set_skin(),
#      the skin's map.png is drawn as the underlay and per-(territory, sector)
#      anchors from the skin replace the procedural centroids. Any territory
#      the skin doesn't cover falls back to procedural for that one piece.
#
# Units render as faction-coloured chips (or, when the skin ships them, force
# token sprites); spice piles render as gold dots; the active storm sector
# overlays in red regardless of mode.

# preload() so the classes are available during const-init (class_name lookup
# is resolved later in the compile pass and breaks the constants below).
const DuneMapData := preload("res://scripts/dune_map_data.gd")
const DuneSkin    := preload("res://scripts/dune_skin.gd")

const SECTOR_COUNT := DuneMapData.SECTOR_COUNT
const RING_COUNT   := DuneMapData.RING_COUNT

# Visual tuning. The renderer is fully procedural — these knobs are the
# only thing you'd touch to redesign the look.
const RING_INNER_RADIUS_FRAC := 0.06   # polar sink radius (× shorter view side)
const RING_OUTER_RADIUS_FRAC := 0.46
const ARC_SUBDIVISIONS       := 6      # tessellation per sector boundary
const TERRITORY_OUTLINE      := Color(0, 0, 0, 0.55)
const TERRITORY_OUTLINE_W    := 1.0
const STORM_COLOR            := Color(0.9, 0.15, 0.15, 0.32)
const SECTOR_GUIDE_COLOR     := Color(0, 0, 0, 0.18)
const TEXT_COLOR             := Color(0.05, 0.05, 0.05, 0.85)
const SPICE_COLOR            := Color(1.0, 0.83, 0.2)

var _snapshot: Dictionary = {}
var _font: Font = null
var _skin: DuneSkin = null

# Cached skin→view transform, recomputed on resize/set_skin.
var _skin_scale: float = 1.0
var _skin_offset: Vector2 = Vector2.ZERO

func _ready() -> void:
	_font = ThemeDB.fallback_font
	resized.connect(_on_resized)

func _on_resized() -> void:
	_recompute_skin_transform()
	queue_redraw()

func set_snapshot(snapshot: Dictionary) -> void:
	_snapshot = snapshot
	queue_redraw()

func set_skin(skin: DuneSkin) -> void:
	_skin = skin
	_recompute_skin_transform()
	queue_redraw()

func _recompute_skin_transform() -> void:
	# Skin coords are in the map image's pixel space (map_size). Fit-to-control
	# with uniform scale + center the result.
	if _skin == null or _skin.map_size == Vector2.ZERO:
		_skin_scale = 1.0
		_skin_offset = Vector2.ZERO
		return
	var sx := size.x / _skin.map_size.x
	var sy := size.y / _skin.map_size.y
	_skin_scale = minf(sx, sy)
	var rendered := _skin.map_size * _skin_scale
	_skin_offset = (size - rendered) * 0.5

func _skin_to_view(p: Vector2) -> Vector2:
	return _skin_offset + p * _skin_scale

func _draw() -> void:
	if _snapshot.is_empty():
		return

	if _skin != null and _skin.has_map_image():
		_draw_skin_background()
	else:
		_draw_procedural_background()

	# Storm overlay + unit/spice glyphs are drawn the same way in both modes —
	# they just use different anchor functions internally.
	_draw_storm_overlay()
	_draw_units_and_spice()
	if _skin == null or not _skin.has_map_image():
		_draw_polar_sink_label()

func _draw_procedural_background() -> void:
	var center := size * 0.5
	var view_min := minf(size.x, size.y)
	var r_outer := view_min * RING_OUTER_RADIUS_FRAC
	var r_inner := view_min * RING_INNER_RADIUS_FRAC
	_draw_territories_procedural(center, r_inner, r_outer)
	_draw_sector_guides(center, r_inner, r_outer)

func _draw_skin_background() -> void:
	# Fill the area around the map with the skin's background colour, then
	# blit the map image at the computed scale/offset.
	draw_rect(Rect2(Vector2.ZERO, size), _skin.background_color)
	var tex := _skin.get_map_texture()
	if tex != null:
		draw_texture_rect(tex, Rect2(_skin_offset, _skin.map_size * _skin_scale), false)

# --- Geometry helpers ---

# Sector index 1..18 → angle range. Sector 1 starts at the top (12 o'clock)
# and the wheel turns clockwise — matching the published board diagrams.
static func _sector_angle(sector: int) -> float:
	return deg_to_rad(-90.0 + (sector - 1) * (360.0 / SECTOR_COUNT))

static func _ring_radii(ring: int, r_inner: float, r_outer: float) -> Vector2:
	# Ring 0 (polar sink) is the disc inside r_inner; rings 1..4 sit between
	# r_inner and r_outer, evenly subdivided.
	if ring <= 0:
		return Vector2(0.0, r_inner)
	var bands := RING_COUNT - 1   # 4 outer bands
	var step := (r_outer - r_inner) / float(bands)
	var lo := r_inner + step * float(ring - 1)
	var hi := lo + step
	return Vector2(lo, hi)

# Build the polygon for a (ring, sectors[]) intersection. The sectors array
# can be either contiguous (e.g. [5,6,7,8]) or wrap (e.g. [18,1]); we sort
# and detect the wrap case so the polygon goes the short way around.
func _territory_polygon(ring: int, sectors_raw: Array, center: Vector2,
		r_inner: float, r_outer: float) -> PackedVector2Array:
	var pts := PackedVector2Array()
	var radii := _ring_radii(ring, r_inner, r_outer)
	var r_lo: float = radii.x
	var r_hi: float = radii.y

	# Polar Sink: full disc.
	if ring == 0:
		var n := SECTOR_COUNT * ARC_SUBDIVISIONS
		for i in range(n + 1):
			var ang := TAU * float(i) / float(n) - PI * 0.5
			pts.push_back(center + Vector2(cos(ang), sin(ang)) * r_hi)
		return pts

	var sectors: Array = sectors_raw.duplicate()
	sectors.sort()
	# Detect the 18↔1 wrap: if max-min > N/2, the contiguous run goes
	# through the wrap point.
	var start_sector: int = sectors[0]
	var end_sector: int = sectors[-1]
	if end_sector - start_sector > SECTOR_COUNT / 2:
		for i in range(1, sectors.size()):
			if int(sectors[i]) - int(sectors[i - 1]) > 1:
				start_sector = int(sectors[i])
				end_sector = int(sectors[i - 1]) + SECTOR_COUNT
				break

	var ang_start := _sector_angle(start_sector)
	var ang_end := _sector_angle(end_sector + 1)

	# Outer arc, start → end
	var subs := (end_sector - start_sector + 1) * ARC_SUBDIVISIONS
	for i in range(subs + 1):
		var t := float(i) / float(subs)
		var ang := lerpf(ang_start, ang_end, t)
		pts.push_back(center + Vector2(cos(ang), sin(ang)) * r_hi)

	# Inner arc, end → start (closes the annular sector)
	for i in range(subs + 1):
		var t := float(i) / float(subs)
		var ang := lerpf(ang_end, ang_start, t)
		pts.push_back(center + Vector2(cos(ang), sin(ang)) * r_lo)

	return pts

# --- Draw passes ---

# Storm overlay: only in procedural mode. In skin mode we don't have a
# canonical (center, radius, zero-angle) without the skin author pinning it
# down — left to the side panel for now.
func _draw_storm_overlay() -> void:
	if _skin != null and _skin.has_map_image():
		return
	var storm: Dictionary = _snapshot.get("storm", {})
	var sector := int(storm.get("sector", 0))
	if sector < 1 or sector > SECTOR_COUNT:
		return
	var center := size * 0.5
	var view_min := minf(size.x, size.y)
	var r_outer := view_min * RING_OUTER_RADIUS_FRAC
	var ang_start := _sector_angle(sector)
	var ang_end := _sector_angle(sector + 1)
	var pts := PackedVector2Array()
	var subs := ARC_SUBDIVISIONS * 2
	for i in range(subs + 1):
		var t := float(i) / float(subs)
		var ang := lerpf(ang_start, ang_end, t)
		pts.push_back(center + Vector2(cos(ang), sin(ang)) * r_outer)
	pts.push_back(center)
	draw_colored_polygon(pts, STORM_COLOR)

func _draw_territories_procedural(center: Vector2, r_inner: float, r_outer: float) -> void:
	var territories: Array = _snapshot.get("map", {}).get("territories", [])
	for t in territories:
		var name: String = t.get("name", "")
		var terrain: String = t.get("terrain", "desert")
		var sectors: Array = t.get("sectors", [])
		var ring := DuneMapData.ring_for(name)
		var poly := _territory_polygon(ring, sectors, center, r_inner, r_outer)
		if poly.is_empty():
			continue
		draw_colored_polygon(poly, DuneMapData.terrain_color(terrain))
		var outline := poly.duplicate()
		outline.push_back(outline[0])
		draw_polyline(outline, TERRITORY_OUTLINE, TERRITORY_OUTLINE_W, true)

func _draw_sector_guides(center: Vector2, r_inner: float, r_outer: float) -> void:
	for s in range(1, SECTOR_COUNT + 1):
		var ang := _sector_angle(s)
		var p_in := center + Vector2(cos(ang), sin(ang)) * r_inner
		var p_out := center + Vector2(cos(ang), sin(ang)) * r_outer
		draw_line(p_in, p_out, SECTOR_GUIDE_COLOR, 1.0, true)

# Anchor for a unit stack at (territory, sector). Prefers the skin's anchor
# transformed into view space; falls back to the procedural centroid.
func _anchor_for(territory_name: String, sectors: Array, sector_hint: int = -1) -> Vector2:
	if _skin != null:
		var s := sector_hint
		if s < 0 and not sectors.is_empty():
			s = int(sectors[0])
		var p = _skin.force_anchor(territory_name, s)
		if p != null:
			return _skin_to_view(p)
	# Procedural fallback.
	var center := size * 0.5
	var view_min := minf(size.x, size.y)
	var r_outer := view_min * RING_OUTER_RADIUS_FRAC
	var r_inner := view_min * RING_INNER_RADIUS_FRAC
	var ring := DuneMapData.ring_for(territory_name)
	return _centroid(ring, sectors, center, r_inner, r_outer)

func _spice_anchor_for(territory_name: String, sectors: Array, sector_hint: int = -1) -> Vector2:
	if _skin != null:
		var s := sector_hint
		if s < 0 and not sectors.is_empty():
			s = int(sectors[0])
		var p = _skin.spice_anchor(territory_name, s)
		if p != null:
			return _skin_to_view(p)
	return _anchor_for(territory_name, sectors, sector_hint)

func _draw_units_and_spice() -> void:
	var territories: Array = _snapshot.get("map", {}).get("territories", [])
	for t in territories:
		var name: String = t.get("name", "")
		var sectors: Array = t.get("sectors", [])
		if sectors.is_empty():
			continue

		var unit_offset := 0
		for u in t.get("units", []):
			var n := int(u.get("normal", 0))
			var e := int(u.get("elite", 0))
			if n + e == 0:
				continue
			var f := int(u.get("faction_index", -1))
			var fac_name := _faction_name_for_index(f)
			var sector := int(u.get("sector", -1))
			var anchor := _anchor_for(name, sectors, sector)
			var pos := anchor + Vector2(0, unit_offset * 14 - 6)
			_draw_unit_chip(pos, f, fac_name, n, e, bool(u.get("advisor", false)))
			unit_offset += 1

		for sp in t.get("spice", []):
			var amt := int(sp.get("amount", 0))
			if amt <= 0:
				continue
			var sector := int(sp.get("sector", -1))
			var spice_pos := _spice_anchor_for(name, sectors, sector) + Vector2(0, unit_offset * 14)
			draw_circle(spice_pos, 7.0, SPICE_COLOR)
			_draw_centred_text(spice_pos, str(amt), 9, Color.BLACK)
			unit_offset += 1

func _draw_unit_chip(pos: Vector2, faction_idx: int, faction_name: String,
		normal: int, elite: int, advisor: bool) -> void:
	# Skin sprite first, color chip otherwise.
	var tex: Texture2D = null
	if _skin != null and not faction_name.is_empty():
		tex = _skin.faction_force_texture(faction_name, elite > 0)
	var bg := DuneMapData.faction_color(faction_idx)
	var fg := DuneMapData.faction_text_color(faction_idx)
	if _skin != null and not faction_name.is_empty():
		bg = _skin.faction_color(faction_name, bg)
		fg = _skin.faction_text_color(faction_name, fg)
	var label := "%d" % normal if elite == 0 else "%d+%d" % [normal, elite]
	if advisor:
		label = "•" + label
	if tex != null:
		# Roughly chip-sized sprite, centered on pos.
		var s := 18.0
		draw_texture_rect(tex, Rect2(pos - Vector2(s, s) * 0.5, Vector2(s, s)), false)
		_draw_centred_text(pos + Vector2(0, s * 0.6), label, 9, fg)
	else:
		draw_circle(pos, 8.5, bg)
		_draw_centred_text(pos, label, 9, fg)

func _draw_polar_sink_label() -> void:
	_draw_centred_text(size * 0.5, "Polar\nSink", 8, TEXT_COLOR)

# Faction index → name lookup. Uses the snapshot's player list so we don't
# have to hardcode the index→name mapping anywhere.
func _faction_name_for_index(idx: int) -> String:
	if idx < 0:
		return ""
	var players: Array = _snapshot.get("players", [])
	for p in players:
		if int(p.get("faction_index", -1)) == idx:
			return str(p.get("faction_name", ""))
	return ""

# --- Geometry: territory centroid (rough — midpoint of the arc midline) ---

func _centroid(ring: int, sectors: Array, center: Vector2,
		r_inner: float, r_outer: float) -> Vector2:
	if ring == 0:
		return center
	var radii := _ring_radii(ring, r_inner, r_outer)
	var r_mid: float = (radii.x + radii.y) * 0.5
	# Average the sector midpoints, with wrap correction (sum unit vectors).
	var v := Vector2.ZERO
	for s_any in sectors:
		var s := int(s_any)
		var ang := _sector_angle(s) + deg_to_rad(360.0 / SECTOR_COUNT) * 0.5
		v += Vector2(cos(ang), sin(ang))
	if v == Vector2.ZERO:
		return center
	v = v.normalized() * r_mid
	return center + v

func _draw_centred_text(pos: Vector2, text: String, font_size: int, color: Color) -> void:
	if _font == null:
		return
	var size_v := _font.get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_CENTER, -1, font_size)
	var ascent := _font.get_ascent(font_size)
	var origin := pos - size_v * 0.5 + Vector2(0, ascent)
	draw_multiline_string(_font, origin, text, HORIZONTAL_ALIGNMENT_CENTER, -1, font_size, -1, color)
