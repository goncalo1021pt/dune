#pragma once

#include <string>
#include <array>

namespace GameConstants {
    constexpr int MIN_PLAYERS = 2;
    constexpr int MAX_PLAYERS = 6;
    constexpr int MAX_TURNS = 10;
    constexpr int TOTAL_SECTORS = 18;
    constexpr int MAX_REVIVES_PER_TURN = 3;
    constexpr int SPICE_COST_PER_REVIVAL = 2;
    constexpr int CHARITY_THRESHOLD = 2;
	constexpr int STARTING_SPICE = 0;
	constexpr int STARTING_UNITS = 20;
    constexpr int STARTING_SPECIAL_UNITS = 0;
	constexpr int MAX_TREACHERY_CARDS = 4;
	constexpr int MAX_TRAITOR_CARDS = 4;
	constexpr int TOKEN_SECTORS[] = {2, 5, 8, 11, 14, 17};
	constexpr int NUM_TOKEN_SECTORS = 6;
	
	// Faction name mapping
	static constexpr std::array<const char*, 6> FACTION_NAMES = {{
		"Atreides",
		"Harkonnen",
		"Fremen",
		"Emperor",
		"Spacing Guild",
		"Bene Gesserit"
	}};
	
	// Helper to get faction name by index
	inline std::string getFactionName(int factionIndex) {
		if (factionIndex >= 0 && factionIndex < (int)FACTION_NAMES.size()) {
			return FACTION_NAMES[factionIndex];
		}
		return "Unknown";
	}
}

struct GameFeatureSettings {
	// Advanced modules (all disabled in default/basic game).
	bool advancedFactionAbilities = false;
	bool increasedSpiceFlow = false;
	bool advancedCombat = false;

	// Concrete toggle currently implemented under Increased Spice Flow.
	bool advancedDoubleSpiceBlow = false;
};

inline GameFeatureSettings defaultFeatureSettings() {
	GameFeatureSettings s;
	s.advancedFactionAbilities = false;
	s.increasedSpiceFlow = false;
	s.advancedCombat = false;
	s.advancedDoubleSpiceBlow = false;
	return s;
}

// Testing profile: enable most currently implemented advanced features.
inline GameFeatureSettings testingFeatureSettings() {
	GameFeatureSettings s;
	s.advancedFactionAbilities = true;
	s.increasedSpiceFlow = true;
	s.advancedCombat = true;
	s.advancedDoubleSpiceBlow = true;
	return s;
}

using namespace GameConstants;