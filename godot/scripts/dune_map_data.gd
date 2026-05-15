extends RefCounted
class_name DuneMapData

# Static layout tables for the Arrakis renderer. The engine source-of-truth
# is `srcs/map.cpp` — territory names + the sectors they occupy come from
# there. The ring assignment is renderer-local: the engine has no concept of
# "ring", but every territory sits at a recognisable distance from the polar
# centre, so we hardcode a band for each.
#
# If a territory is missing from RING, the renderer drops it into the
# outermost ring with a warning — keeps the view from crashing on a stale
# layout while the engine map evolves.

const RING_POLAR_SINK := 0
const RING_INNER      := 1
const RING_MID_INNER  := 2
const RING_MID_OUTER  := 3
const RING_OUTER      := 4
const RING_COUNT      := 5

# Territory → ring. Eyeballed against the Avalon Hill 1979 board layout;
# good enough for a recognisable Arrakis at this fidelity.
const RING := {
	"Polar Sink":          RING_POLAR_SINK,

	"Imperial Basin":      RING_INNER,
	"Arrakeen":            RING_INNER,
	"Carthag":             RING_INNER,

	"Old Gap":             RING_MID_INNER,
	"Tsimpo":              RING_MID_INNER,
	"Hagga Basin":         RING_MID_INNER,
	"Plastic Basin":       RING_MID_INNER,
	"Sihaya Ridge":        RING_MID_INNER,
	"Hole in the Rock":    RING_MID_INNER,
	"Basin":               RING_MID_INNER,
	"Broken Land":         RING_MID_INNER,
	"Arsunt":              RING_MID_INNER,
	"Rock Outcroppings":   RING_MID_INNER,

	"Shield Wall":         RING_MID_OUTER,
	"False Wall East":     RING_MID_OUTER,
	"False Wall South":    RING_MID_OUTER,
	"False Wall West":     RING_MID_OUTER,
	"Pasty Mesa":          RING_MID_OUTER,
	"The Minor Erg":       RING_MID_OUTER,
	"Red Chasm":           RING_MID_OUTER,
	"Gara Kulon":          RING_MID_OUTER,
	"Rim Wall West":       RING_MID_OUTER,
	"Sietch Tabr":         RING_MID_OUTER,
	"Habbanya Sietch":     RING_MID_OUTER,
	"Tuek's Sietch":       RING_MID_OUTER,
	"Bight of the Cliff":  RING_MID_OUTER,

	"Cielago West":        RING_OUTER,
	"Cielago Depression":  RING_OUTER,
	"Cielago North":       RING_OUTER,
	"Cielago South":       RING_OUTER,
	"Cielago East":        RING_OUTER,
	"Meridian":            RING_OUTER,
	"Harg Pass":           RING_OUTER,
	"South Mesa":          RING_OUTER,
	"Wind Pass":           RING_OUTER,
	"Wind Pass North":     RING_OUTER,
	"Habbanya Erg":        RING_OUTER,
	"Habbanya Ridge Flat": RING_OUTER,
	"The Great Flat":      RING_OUTER,
	"Funeral Plain":       RING_OUTER,
	"The Greater Flat":    RING_OUTER,
}

const TERRAIN_COLOR := {
	"city":      Color("c8a96b"),
	"rock":      Color("8a7355"),
	"desert":    Color("d8b88a"),
	"northPole": Color("4a7896"),
}

# Faction palette. Indices match the Game's player_count loop:
#   0 Atreides, 1 Harkonnen, 2 Fremen, 3 Emperor, 4 Spacing Guild, 5 Bene Gesserit.
const FACTION_COLOR := [
	Color("2e5cb8"),   # Atreides    — blue
	Color("1a1a1a"),   # Harkonnen   — black
	Color("d27a36"),   # Fremen      — orange/desert
	Color("9b2742"),   # Emperor     — crimson
	Color("d6c14a"),   # Guild       — gold
	Color("efe9d6"),   # Bene Gesserit — bone
]

const FACTION_TEXT_COLOR := [
	Color.WHITE,
	Color.WHITE,
	Color.WHITE,
	Color.WHITE,
	Color.BLACK,
	Color.BLACK,
]

const SECTOR_COUNT := 18

static func ring_for(territory_name: String) -> int:
	if RING.has(territory_name):
		return RING[territory_name]
	push_warning("DuneMapData: no ring assigned for territory '%s' — using outer." % territory_name)
	return RING_OUTER

static func terrain_color(terrain: String) -> Color:
	return TERRAIN_COLOR.get(terrain, Color.MAGENTA)

static func faction_color(idx: int) -> Color:
	if idx >= 0 and idx < FACTION_COLOR.size():
		return FACTION_COLOR[idx]
	return Color.GRAY

static func faction_text_color(idx: int) -> Color:
	if idx >= 0 and idx < FACTION_TEXT_COLOR.size():
		return FACTION_TEXT_COLOR[idx]
	return Color.WHITE
