#include "phases/storm_phase.hpp"
#include "factions/faction_ability.hpp"
#include "map.hpp"
#include "player.hpp"
#include "interaction/interaction_adapter.hpp"
#include <algorithm>
#include <set>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

namespace {

bool hasCard(const Player* player, const std::string& cardName) {
	const auto& cards = player->getTreacheryCards();
	return std::find(cards.begin(), cards.end(), cardName) != cards.end();
}

} // namespace

void StormPhase::initializeStormDeck(std::vector<int>& stormDeck, std::mt19937& rng) {
	stormDeck = {1, 2, 3, 4, 5, 6};
	std::shuffle(stormDeck.begin(), stormDeck.end(), rng);
}

int StormPhase::drawStormCard(std::vector<int>& stormDeck, std::mt19937& rng) {
	int card = stormDeck[0];
	initializeStormDeck(stormDeck, rng);
	return card;
}

void StormPhase::moveStorm(int sectorsToMove, int& stormSector) {
	stormSector += sectorsToMove;
	while (stormSector > TOTAL_SECTORS) stormSector -= TOTAL_SECTORS;
	while (stormSector < 1)             stormSector += TOTAL_SECTORS;
}

void StormPhase::applyStormDamage(PhaseContext& ctx, int prevSector, int move) {
	// Build the full set of sectors swept this turn (all sectors passed through
	// AND the final stopping sector).
	std::set<int> swept = GameMap::getStormSweep(prevSector, move);

	const std::vector<territory>& allTerritories = ctx.map.getTerritories();
	
	for (const auto& constTerr : allTerritories) {
		// Get mutable reference to this territory
		territory* terr = ctx.map.getTerritory(constTerr.name);
		if (terr == nullptr) continue;

		// Polar Sink is never affected by storm.
		if (terr->terrain == terrainType::northPole) continue;
		// Rock and city territories are always storm-safe.
		if (terr->terrain == terrainType::rock)      continue;
		if (terr->terrain == terrainType::city)      continue;

		// Which sectors of this territory are inside the swept set?
		std::set<int> damagedSectors;
		for (int s : terr->sectors) {
			if (swept.count(s)) damagedSectors.insert(s);
		}
		if (damagedSectors.empty()) continue;

		// Process each unit stack. Stacks in safe sectors are untouched.
		// Stacks in damaged sectors are killed (or halved for Fremen).
		std::vector<unitStack> survivors;
		int totalKilled = 0;

		for (auto& stack : terr->unitsPresent) {
			if (!damagedSectors.count(stack.sector)) {
				// Safe sector — keep this stack unchanged.
				survivors.push_back(stack);
				continue;
			}

			FactionAbility* ability = ctx.getAbility(stack.factionOwner);

			if (ctx.featureSettings.advancedFactionAbilities && ability && ability->hasReducedStormLosses()) {
				// Fremen: kill half (round up), survivors stay in same sector.
				int killedNormal = (stack.normal_units + 1) / 2;
				int killedElite  = (stack.elite_units  + 1) / 2;
				stack.normal_units -= killedNormal;
				stack.elite_units  -= killedElite;
				if (killedNormal > 0) {
					ctx.players[stack.factionOwner]->destroyUnits(killedNormal);
				}
				if (killedElite > 0) {
					ctx.players[stack.factionOwner]->destroyEliteUnits(killedElite);
				}
				totalKilled += killedNormal + killedElite;

				if (ctx.logger && (killedNormal + killedElite) > 0) {
					ctx.logger->logDebug("Storm (sector " + std::to_string(stack.sector) +
						"): Fremen lose " + std::to_string(killedNormal + killedElite) +
						" units in " + terr->name + " (half loss)");
				}

				if (stack.normal_units > 0 || stack.elite_units > 0) {
					survivors.push_back(stack);
				}
			} else {
				// Everyone else: entire stack in the damaged sector is destroyed.
				int killedNormal = stack.normal_units;
				int killedElite = stack.elite_units;
				if (killedNormal > 0) {
					ctx.players[stack.factionOwner]->destroyUnits(killedNormal);
				}
				if (killedElite > 0) {
					ctx.players[stack.factionOwner]->destroyEliteUnits(killedElite);
				}
				int killed = killedNormal + killedElite;
				totalKilled += killed;

				if (ctx.logger && killed > 0) {
					ctx.logger->logDebug("Storm (sector " + std::to_string(stack.sector) +
						"): " + ctx.players[stack.factionOwner]->getFactionName() +
						" loses " + std::to_string(killed) +
						" units in " + terr->name);
				}
				// Stack is not pushed to survivors — it is gone.
			}
		}

		terr->unitsPresent = survivors;

		// Remove spice from any territory that had at least one damaged sector.
		// Rulebook: spice in a sector the storm passes through is removed.
		// Since spice now tracks sectors, we remove spice from damaged sectors.
		int spiceLost = ctx.map.getSpiceInTerritory(terr->name);
		if (spiceLost > 0) {
			ctx.map.removeAllSpiceFromTerritory(terr->name);

			if (ctx.logger) {
				Event e(EventType::SPICE_BLOWN,
					"Storm destroys " + std::to_string(spiceLost) + " spice in " + terr->name,
					ctx.turnNumber, "STORM");
				e.territory  = terr->name;
				e.spiceValue = spiceLost;
				ctx.logger->logEvent(e);
			}
		}

		if (ctx.logger && totalKilled > 0) {
			Event e(EventType::UNITS_KILLED,
				"Storm kills " + std::to_string(totalKilled) + " units in " + terr->name,
				ctx.turnNumber, "STORM");
			e.territory  = terr->name;
			e.unitCount  = totalKilled;
			ctx.logger->logEvent(e);
		}
	}
}

void StormPhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("STORM Phase");
	}
	auto view = ctx.getStormView();

	auto logFremenStormPrescience = [&]() {
		if (!ctx.logger || !view.hasNextStormCard) {
			return;
		}
		bool hasFremen = false;
		for (int i = 0; i < ctx.playerCount; ++i) {
			FactionAbility* ability = ctx.players[i]->getFactionAbility();
			if (ability && ability->getFactionName() == "Fremen") {
				hasFremen = true;
				break;
			}
		}
		if (hasFremen) {
			ctx.logger->logDebug("[Fremen Prescience] Next turn storm move card: " + std::to_string(view.nextStormCard));
		}
	};

	if (view.turnNumber == 1) {
		// Turn 1: place storm at a random starting sector.
		// No units have been placed yet so damage application is skipped.
		std::uniform_int_distribution<> startSectorDist(1, 18);
		view.stormSector     = startSectorDist(view.rng);
		view.lastStormCard   = 0;
		view.nextStormCard   = drawStormCard(view.stormDeck, view.rng);
		view.hasNextStormCard = true;

		if (ctx.logger) {
			Event e(EventType::STORM_PLACED,
				"Storm placed at sector " + std::to_string(view.stormSector),
				ctx.turnNumber, "STORM");
			e.territory = std::to_string(view.stormSector);
			ctx.logger->logEvent(e);
		}
		logFremenStormPrescience();
		return;
	}

	// Subsequent turns: draw card, move storm, apply damage to swept sectors.
	if (!view.hasNextStormCard) {
		view.nextStormCard    = drawStormCard(view.stormDeck, view.rng);
		view.hasNextStormCard = true;
	}

	int prevSector        = view.stormSector;
	view.lastStormCard    = view.nextStormCard;
	view.hasNextStormCard = false;

	int weatherController = -1;
	if (view.turnNumber > 1) {
		for (int idx : ctx.turnOrder) {
			if (!hasCard(ctx.players[idx], "Weather Control")) {
				continue;
			}

			bool play = false;
			if (ctx.adapter) {
				DecisionRequest req;
				req.kind = "yn";
				req.actor_index = idx;
				req.prompt = ctx.players[idx]->getFactionName() +
					", play Weather Control to set storm movement (0-10)?";
				auto resp = ctx.adapter->requestDecision(req);
				play = resp && resp->valid && resp->payload_json == "y";
			}

			if (!play) {
				continue;
			}

			int chosenMove = view.lastStormCard;
			if (ctx.adapter) {
				DecisionRequest req;
				req.kind = "int";
				req.actor_index = idx;
				req.prompt = "Enter storm movement sectors (0-10): ";
				req.int_min = 0;
				req.int_max = 10;
				auto resp = ctx.adapter->requestDecision(req);
				if (resp && resp->valid) {
					try { chosenMove = std::stoi(resp->payload_json); } catch (...) {}
				}
			} else {
				std::uniform_int_distribution<> dist(0, 10);
				chosenMove = dist(view.rng);
			}

			ctx.players[idx]->removeTreacheryCard("Weather Control");
			view.lastStormCard = chosenMove;
			weatherController = idx;
			break;
		}
	}

	moveStorm(view.lastStormCard, view.stormSector);

	// Pre-fetch next card for the next turn.
	view.nextStormCard    = drawStormCard(view.stormDeck, view.rng);
	view.hasNextStormCard = true;

	if (ctx.logger) {
		std::string movementMsg;
		if (weatherController >= 0) {
			movementMsg = ctx.players[weatherController]->getFactionName() +
				" uses Weather Control. Storm moves " + std::to_string(view.lastStormCard);
		} else {
			movementMsg = "Storm moves " + std::to_string(view.lastStormCard);
		}

		Event e(EventType::STORM_MOVED,
			movementMsg +
			" sectors (was " + std::to_string(prevSector) +
			", now sector " + std::to_string(view.stormSector) + ")",
			ctx.turnNumber, "STORM");
		e.spiceValue = view.lastStormCard;
		ctx.logger->logEvent(e);
	}

	// Apply damage to all sectors swept this turn.
	applyStormDamage(ctx, prevSector, view.lastStormCard);
	logFremenStormPrescience();
}
