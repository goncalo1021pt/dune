#include "ffi/snapshot_builder.hpp"

#include "game.hpp"
#include "player.hpp"
#include "map.hpp"
#include "leader.hpp"
#include "cards/treachery_deck.hpp"
#include "factions/harkonnen_ability.hpp"
#include "factions/bene_gesserit_ability.hpp"
#include "reactions/reaction_engine.hpp"
#include "reactions/reaction_window.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

const char* phaseName(gamePhase p) {
	switch (p) {
		case gamePhase::STORM:            return "STORM";
		case gamePhase::SPICE_BLOW:       return "SPICE_BLOW";
		case gamePhase::CHOAM_CHARITY:    return "CHOAM_CHARITY";
		case gamePhase::BIDDING:          return "BIDDING";
		case gamePhase::REVIVAL:          return "REVIVAL";
		case gamePhase::SHIP_AND_MOVE:    return "SHIP_AND_MOVE";
		case gamePhase::BATTLE:           return "BATTLE";
		case gamePhase::SPICE_COLLECTION: return "SPICE_COLLECTION";
		case gamePhase::MENTAT_PAUSE:     return "MENTAT_PAUSE";
		case gamePhase::COUNT:            return "COUNT";
	}
	return "UNKNOWN";
}

const char* terrainName(terrainType t) {
	switch (t) {
		case terrainType::desert:    return "desert";
		case terrainType::rock:      return "rock";
		case terrainType::city:      return "city";
		case terrainType::northPole: return "northPole";
	}
	return "unknown";
}

json buildLeaderJson(const Leader& l) {
	return json{
		{"name", l.name},
		{"power", l.power},
		{"has_battled", l.hasBattled},
	};
}

json buildPlayerJson(const Player& p) {
	json leadersAlive = json::array();
	for (const Leader& l : p.getAliveLeaders()) leadersAlive.push_back(buildLeaderJson(l));
	json leadersDead = json::array();
	for (const Leader& l : p.getDeadLeaders())  leadersDead.push_back(buildLeaderJson(l));

	json out = {
		{"faction_index",     p.getFactionIndex()},
		{"faction_name",      p.getFactionName()},
		{"home_sector",       p.getHomeSector()},
		{"spice",             p.getSpice()},
		{"reserve",           {{"normal", p.getUnitsReserve()},   {"elite", p.getEliteUnitsReserve()}}},
		{"deployed",          {{"normal", p.getUnitsDeployed()},  {"elite", p.getEliteUnitsDeployed()}}},
		{"destroyed",         {{"normal", p.getUnitsDestroyed()}, {"elite", p.getEliteUnitsDestroyed()}}},
		{"free_revives",      p.getFreeRevivesPerTurn()},
		{"treachery_cards",   p.getTreacheryCards()},
		{"traitor_cards",     p.getTraitorCards()},
		{"leaders_alive",     leadersAlive},
		{"leaders_dead",      leadersDead},
	};

	// Faction-specific persistent state. Cast to the ability classes that
	// hold extra state; everyone else (Atreides, Fremen, Emperor, Guild)
	// has nothing beyond the base Player fields.
	const FactionAbility* ability = p.getFactionAbility();
	if (auto* hark = dynamic_cast<const HarkonnenAbility*>(ability)) {
		json captured = json::array();
		for (const Leader& l : hark->getCapturedLeaders()) captured.push_back(buildLeaderJson(l));
		out["captured_leaders"] = captured;
	}
	if (auto* bg = dynamic_cast<const BeneGesseritAbility*>(ability)) {
		out["prediction"] = {
			{"set",       bg->hasPrediction()},
			{"faction",   bg->getPredictedFaction()},
			{"turn",      bg->getPredictedTurn()},
		};
	}

	return out;
}

json buildUnitStackJson(const unitStack& s) {
	return json{
		{"faction_index", s.factionOwner},
		{"normal",        s.normal_units},
		{"elite",         s.elite_units},
		{"advisor",       s.advisor},
		{"sector",        s.sector},
	};
}

json buildSpiceStackJson(const spiceStack& s) {
	return json{
		{"amount", s.amount},
		{"sector", s.sector},
	};
}

json buildTerritoryJson(const territory& t) {
	json units = json::array();
	for (const unitStack& s : t.unitsPresent) units.push_back(buildUnitStackJson(s));
	json spices = json::array();
	for (const spiceStack& s : t.spicePresent) spices.push_back(buildSpiceStackJson(s));

	return json{
		{"name",             t.name},
		{"terrain",          terrainName(t.terrain)},
		{"sectors",          t.sectors},
		{"has_transporter",  t.hasTransporter},
		{"special_movement", t.specialMovement},
		{"units",            units},
		{"spice",            spices},
	};
}

} // namespace

namespace SnapshotBuilder {

std::string buildSnapshotJson(const Game& game) {
	json out;

	out["schema_version"] = 1;

	out["game"] = {
		{"turn",            game.getTurnNumber()},
		{"phase",           phaseName(game.getCurrentPhase())},
		{"turn_order",      game.getTurnOrder()},
		{"player_count",    game.getPlayerCount()},
		{"game_ended",      game.isGameEnded()},
		{"initial_seed",    game.getInitialSeed()},
		{"interactive",     game.isInteractiveMode()},
	};

	const GameFeatureSettings& fs = game.getFeatureSettings();
	out["feature_settings"] = {
		{"advanced_faction_abilities", fs.advancedFactionAbilities},
		{"increased_spice_flow",       fs.increasedSpiceFlow},
		{"advanced_combat",            fs.advancedCombat},
		{"advanced_double_spice_blow", fs.advancedDoubleSpiceBlow},
	};

	out["storm"] = {
		{"sector",          game.getStormSector()},
		{"last_card",       game.getLastStormCard()},
		{"next_card",       game.getNextStormCard()},
	};

	json players = json::array();
	for (int i = 0; i < game.getPlayerCount(); ++i) {
		const Player* p = game.getPlayer(i);
		if (p) players.push_back(buildPlayerJson(*p));
	}
	out["players"] = players;

	json territories = json::array();
	for (const territory& t : game.getTerritories()) territories.push_back(buildTerritoryJson(t));
	out["map"] = {{"territories", territories}};

	out["treachery_discard"] = game.getTreacheryDeck().getDiscardPile();

	out["reactions"] = {
		{"current_window", reactionWindowName(game.getReactionEngine().currentWindow())},
		{"window_open",    game.getReactionEngine().isWindowOpen()},
	};

	return out.dump();
}

} // namespace SnapshotBuilder
