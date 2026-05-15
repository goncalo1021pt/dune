extends Control
class_name DuneMapView

# Polar map renderer for an Arrakis snapshot.
#
# Layout: 18 storm sectors × 5 rings (centre = Polar Sink). Each territory
# claims a (ring, sector-set) intersection and is drawn as an annular sector
# polygon. Units are stacked as a small chip at the territory's centroid;
# spice piles render as gold dots; the active storm sector overlays in red.

# preload() so the class is available during const-init (class_name lookup
# is resolved later in the compile pass and breaks the constants below).
const DuneMapData := preload("res://scripts/dune_map_data.gd")

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

func _ready() -> void:
	_font = ThemeDB.fallback_font
	resized.connect(queue_redraw)

func set_snapshot(snapshot: Dictionary) -> void:
	_snapshot = snapshot
	queue_redraw()

func _draw() -> void:
	if _snapshot.is_empty():
		return

	var center := size * 0.5
	var view_min := minf(size.x, size.y)
	var radius_outer := view_min * RING_OUTER_RADIUS_FRAC
	var radius_inner := view_min * RING_INNER_RADIUS_FRAC

	_draw_storm_overlay(center, radius_inner, radius_outer)
	_draw_territories(center, radius_inner, radius_outer)
	_draw_sector_guides(center, radius_inner, radius_outer)
	_draw_units_and_spice(center, radius_inner, radius_outer)
	_draw_polar_sink_label(center, radius_inner)

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

func _draw_storm_overlay(center: Vector2, r_inner: float, r_outer: float) -> void:
	var storm: Dictionary = _snapshot.get("storm", {})
	var sector := int(storm.get("sector", 0))
	if sector < 1 or sector > SECTOR_COUNT:
		return
	var ang_start := _sector_angle(sector)
	var ang_end := _sector_angle(sector + 1)
	var pts := PackedVector2Array()
	var subs := ARC_SUBDIVISIONS * 2
	for i in range(subs + 1):
		var t := float(i) / float(subs)
		var ang := lerpf(ang_start, ang_end, t)
		pts.push_back(center + Vector2(cos(ang), sin(ang)) * r_outer)
	for i in range(subs + 1):
		var t := float(i) / float(subs)
		var ang := lerpf(ang_end, ang_start, t)
		pts.push_back(center + Vector2(cos(ang), sin(ang)) * 0.0)
	draw_colored_polygon(pts, STORM_COLOR)

func _draw_territories(center: Vector2, r_inner: float, r_outer: float) -> void:
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
		# Outline.
		var outline := poly.duplicate()
		outline.push_back(outline[0])
		draw_polyline(outline, TERRITORY_OUTLINE, TERRITORY_OUTLINE_W, true)

func _draw_sector_guides(center: Vector2, r_inner: float, r_outer: float) -> void:
	for s in range(1, SECTOR_COUNT + 1):
		var ang := _sector_angle(s)
		var p_in := center + Vector2(cos(ang), sin(ang)) * r_inner
		var p_out := center + Vector2(cos(ang), sin(ang)) * r_outer
		draw_line(p_in, p_out, SECTOR_GUIDE_COLOR, 1.0, true)

func _draw_units_and_spice(center: Vector2, r_inner: float, r_outer: float) -> void:
	var territories: Array = _snapshot.get("map", {}).get("territories", [])
	for t in territories:
		var name: String = t.get("name", "")
		var sectors: Array = t.get("sectors", [])
		var ring := DuneMapData.ring_for(name)
		if sectors.is_empty():
			continue
		var anchor := _centroid(ring, sectors, center, r_inner, r_outer)

		var unit_offset := 0
		for u in t.get("units", []):
			var f := int(u.get("faction_index", -1))
			var n := int(u.get("normal", 0))
			var e := int(u.get("elite", 0))
			if n + e == 0:
				continue
			var pos := anchor + Vector2(0, unit_offset * 14 - 6)
			_draw_unit_chip(pos, f, n, e, bool(u.get("advisor", false)))
			unit_offset += 1

		for sp in t.get("spice", []):
			var amt := int(sp.get("amount", 0))
			if amt <= 0:
				continue
			var spice_pos := anchor + Vector2(0, unit_offset * 14)
			draw_circle(spice_pos, 7.0, SPICE_COLOR)
			_draw_centred_text(spice_pos, str(amt), 9, Color.BLACK)
			unit_offset += 1

func _draw_unit_chip(pos: Vector2, faction_idx: int, normal: int, elite: int, advisor: bool) -> void:
	var bg := DuneMapData.faction_color(faction_idx)
	var fg := DuneMapData.faction_text_color(faction_idx)
	var label := "%d" % normal if elite == 0 else "%d+%d" % [normal, elite]
	if advisor:
		label = "•" + label
	draw_circle(pos, 8.5, bg)
	_draw_centred_text(pos, label, 9, fg)

func _draw_polar_sink_label(center: Vector2, r_inner: float) -> void:
	_draw_centred_text(center, "Polar\nSink", 8, TEXT_COLOR)

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
