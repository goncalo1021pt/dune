#include <phases/battle_phase.hpp>
#include <phases/phase_context.hpp>
#include <player.hpp>
#include <leader.hpp>
#include <map.hpp>
#include <cards/treachery_deck.hpp>
#include "interaction/interaction_adapter.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cctype>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

namespace {

treacheryCardType getTreacheryTypeFromName(const std::string& cardName) {
	if (cardName == "Crysknife" || cardName == "Maula Pistol" ||
		cardName == "Slip Tip" || cardName == "Stunner") {
		return treacheryCardType::WEAPON_PROJECTILE;
	}
	if (cardName == "Chaumas" || cardName == "Chaumurky" ||
		cardName == "Ellaca Drug" || cardName == "Gom Jabbar") {
		return treacheryCardType::WEAPON_POISON;
	}
	if (cardName == "Lasgun") {
		return treacheryCardType::WEAPON_SPECIAL;
	}
	if (cardName == "Shield") {
		return treacheryCardType::DEFENSE_PROJECTILE;
	}
	if (cardName == "Snooper") {
		return treacheryCardType::DEFENSE_POISON;
	}
	if (cardName == "Cheap Hero") {
		return treacheryCardType::SPECIAL_LEADER;
	}
	if (cardName == "Baliset" || cardName == "Jubba Cloak" || cardName == "Kulon" ||
		cardName == "La La La" || cardName == "Trip to Gamont") {
		return treacheryCardType::WORTHLESS;
	}
	return treacheryCardType::SPECIAL;
}

bool isWorthlessCard(const std::string& cardName) {
	return getTreacheryTypeFromName(cardName) == treacheryCardType::WORTHLESS;
}

bool isOffenseCard(const std::string& cardName) {
	treacheryCardType t = getTreacheryTypeFromName(cardName);
	return t == treacheryCardType::WEAPON_PROJECTILE ||
		       t == treacheryCardType::WEAPON_POISON ||
		       t == treacheryCardType::WEAPON_SPECIAL ||
		       t == treacheryCardType::WORTHLESS;
}

bool isDefenseCard(const std::string& cardName) {
	treacheryCardType t = getTreacheryTypeFromName(cardName);
	return t == treacheryCardType::DEFENSE_PROJECTILE ||
	       t == treacheryCardType::DEFENSE_POISON ||
	       t == treacheryCardType::WORTHLESS;
}

std::vector<std::string> collectBattleCards(const Player* player, bool offense) {
	std::vector<std::string> cards;
	for (const std::string& card : player->getTreacheryCards()) {
		if (offense && isOffenseCard(card)) {
			cards.push_back(card);
		} else if (!offense && isDefenseCard(card)) {
			cards.push_back(card);
		}
	}
	return cards;
}

void removeOneCardInstance(std::vector<std::string>& cards, const std::string& cardName) {
	if (cardName.empty()) return;
	auto it = std::find(cards.begin(), cards.end(), cardName);
	if (it != cards.end()) {
		cards.erase(it);
	}
}

std::string selectBattleCard(PhaseContext& ctx, Player* player,
	const std::vector<std::string>& available,
	const std::string& label,
	bool allowNone = true) {
	if (available.empty()) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " has no " + label + " cards.");
		}
		return "";
	}

	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "select";
		req.actor_index = -1;  // caller sets player index implicitly via player ptr
		req.prompt = player->getFactionName() + ", choose a " + label + " card:";
		req.options = available;
		req.allow_none = allowNone;
		auto resp = ctx.adapter->requestDecision(req);
		return (resp && resp->valid) ? resp->payload_json : "";
	}

	// AI / non-interactive fallback: pick first available.
	auto view = ctx.getBattleView();
	if (!view.interactiveMode) {
		return available[0];
	}
	return available[0];
}

bool weaponKillsLeader(const std::string& weaponCard, const std::string& defenseCard) {
	if (weaponCard.empty()) {
		return false;
	}

	treacheryCardType weaponType = getTreacheryTypeFromName(weaponCard);
	treacheryCardType defenseType = getTreacheryTypeFromName(defenseCard);

	if (weaponType == treacheryCardType::WEAPON_SPECIAL) {
		return true;
	}
	if (weaponType == treacheryCardType::WEAPON_PROJECTILE) {
		return defenseType != treacheryCardType::DEFENSE_PROJECTILE;
	}
	if (weaponType == treacheryCardType::WEAPON_POISON) {
		return defenseType != treacheryCardType::DEFENSE_POISON;
	}
	return false;
}

void killLeaderByName(Player* player, const std::string& leaderName) {
	std::vector<Leader>& alive = player->getAliveLeadersMutable();
	for (size_t i = 0; i < alive.size(); ++i) {
		if (alive[i].name == leaderName) {
			player->killLeader(i);
			return;
		}
	}
}

bool hasCheapHeroCard(const Player* player) {
	for (const std::string& card : player->getTreacheryCards()) {
		if (card == "Cheap Hero") {
			return true;
		}
	}
	return false;
}

struct BattleLeaderChoice {
	bool valid = false;
	bool usedCheapHero = false;
	int aliveLeaderIndex = -1;
	std::string leaderName;
	int leaderPower = 0;
};

BattleLeaderChoice selectBattleLeaderChoice(PhaseContext& ctx, Player* player,
	bool forceCheapHero = false, bool forbidCheapHero = false) {
	BattleLeaderChoice choice;
	const auto& leaders = player->getAliveLeaders();
	const bool cheapHeroAvailable = hasCheapHeroCard(player) && !forbidCheapHero;

	if (leaders.empty() && !cheapHeroAvailable) {
		return choice;
	}

	if (forceCheapHero) {
		if (!hasCheapHeroCard(player)) {
			return choice;
		}
		choice.valid = true;
		choice.usedCheapHero = true;
		choice.leaderName = "Cheap Hero";
		choice.leaderPower = 0;
		return choice;
	}

	auto view = ctx.getBattleView();
	if (!view.interactiveMode) {
		if (!leaders.empty()) {
			int bestIdx = 0;
			int bestPower = -1;
			for (int i = 0; i < static_cast<int>(leaders.size()); ++i) {
				if (leaders[i].power > bestPower) {
					bestPower = leaders[i].power;
					bestIdx = i;
				}
			}
			choice.valid = true;
			choice.aliveLeaderIndex = bestIdx;
			choice.leaderName = leaders[bestIdx].name;
			choice.leaderPower = leaders[bestIdx].power;
		} else {
			choice.valid = true;
			choice.usedCheapHero = true;
			choice.leaderName = "Cheap Hero";
			choice.leaderPower = 0;
		}
		return choice;
	}

	if (ctx.adapter) {
		std::vector<std::string> options;
		for (const auto& l : leaders) {
			options.push_back(l.name + " (power:" + std::to_string(l.power) + ")");
		}
		if (cheapHeroAvailable) {
			options.push_back("Cheap Hero (power:0)");
		}

		DecisionRequest req;
		req.kind = "select";
		req.prompt = player->getFactionName() + ", select leader for battle:";
		req.options = options;
		req.allow_none = false;
		auto resp = ctx.adapter->requestDecision(req);
		if (!resp || !resp->valid || resp->payload_json.empty()) {
			return choice;
		}

		// The response is the option string; match it back to a leader.
		for (int i = 0; i < static_cast<int>(leaders.size()); ++i) {
			const std::string optStr = leaders[i].name + " (power:" + std::to_string(leaders[i].power) + ")";
			if (resp->payload_json == optStr) {
				choice.valid = true;
				choice.aliveLeaderIndex = i;
				choice.leaderName = leaders[i].name;
				choice.leaderPower = leaders[i].power;
				return choice;
			}
		}
		if (cheapHeroAvailable && resp->payload_json == "Cheap Hero (power:0)") {
			choice.valid = true;
			choice.usedCheapHero = true;
			choice.leaderName = "Cheap Hero";
			choice.leaderPower = 0;
		}
		return choice;
	}
	return choice;
}

enum class BattleElementToPeek {
	LEADER = 1,
	WEAPON = 2,
	DEFENSE = 3,
	DIAL = 4
};

struct AtreidesPeekState {
	bool active = false;
	BattleElementToPeek element = BattleElementToPeek::LEADER;
	int atreidesIdx = -1;
	int opponentIdx = -1;
};

struct LockedBattleElement {
	bool active = false;
	BattleElementToPeek element = BattleElementToPeek::LEADER;
	BattleLeaderChoice leaderChoice;
	bool hasLeader = false;
	std::string weapon;
	bool hasWeapon = false;
	std::string defense;
	bool hasDefense = false;
	int dialValue = 0;
	bool hasDial = false;
};

enum class BeneVoiceTarget {
	WEAPON_POISON,
	WEAPON_PROJECTILE,
	WEAPON_LASGUN,
	DEFENSE_SHIELD,
	DEFENSE_SNOOPER,
	WORTHLESS,
	CHEAP_HERO
};

struct BeneVoiceState {
	bool active = false;
	int beneIdx = -1;
	int targetIdx = -1;
	bool demandPlay = false;
	BeneVoiceTarget target = BeneVoiceTarget::WEAPON_POISON;
	bool worthlessSatisfied = false;
};

std::string beneVoiceTargetToString(BeneVoiceTarget target) {
	switch (target) {
		case BeneVoiceTarget::WEAPON_POISON: return "poison weapon";
		case BeneVoiceTarget::WEAPON_PROJECTILE: return "projectile weapon";
		case BeneVoiceTarget::WEAPON_LASGUN: return "lasgun";
		case BeneVoiceTarget::DEFENSE_SHIELD: return "shield";
		case BeneVoiceTarget::DEFENSE_SNOOPER: return "snooper";
		case BeneVoiceTarget::WORTHLESS: return "worthless card";
		case BeneVoiceTarget::CHEAP_HERO: return "cheap hero";
	}
	return "unknown";
}

bool cardMatchesVoiceTarget(const std::string& card, BeneVoiceTarget target) {
	treacheryCardType t = getTreacheryTypeFromName(card);
	switch (target) {
		case BeneVoiceTarget::WEAPON_POISON: return t == treacheryCardType::WEAPON_POISON;
		case BeneVoiceTarget::WEAPON_PROJECTILE: return t == treacheryCardType::WEAPON_PROJECTILE;
		case BeneVoiceTarget::WEAPON_LASGUN: return card == "Lasgun";
		case BeneVoiceTarget::DEFENSE_SHIELD: return card == "Shield";
		case BeneVoiceTarget::DEFENSE_SNOOPER: return card == "Snooper";
		case BeneVoiceTarget::WORTHLESS: return t == treacheryCardType::WORTHLESS;
		case BeneVoiceTarget::CHEAP_HERO: return card == "Cheap Hero";
	}
	return false;
}

bool anyMatchingCard(const std::vector<std::string>& cards, BeneVoiceTarget target) {
	for (const std::string& c : cards) {
		if (cardMatchesVoiceTarget(c, target)) return true;
	}
	return false;
}

bool canComplyWithBeneVoice(const Player* targetPlayer, const BeneVoiceState& voice) {
	if (!voice.active || !voice.demandPlay) return true;

	if (voice.target == BeneVoiceTarget::CHEAP_HERO) {
		return hasCheapHeroCard(targetPlayer);
	}

	if (voice.target == BeneVoiceTarget::WORTHLESS) {
		std::vector<std::string> offense = collectBattleCards(targetPlayer, true);
		std::vector<std::string> defense = collectBattleCards(targetPlayer, false);
		return anyMatchingCard(offense, voice.target) || anyMatchingCard(defense, voice.target);
	}

	if (voice.target == BeneVoiceTarget::DEFENSE_SHIELD || voice.target == BeneVoiceTarget::DEFENSE_SNOOPER) {
		std::vector<std::string> defense = collectBattleCards(targetPlayer, false);
		return anyMatchingCard(defense, voice.target);
	}

	std::vector<std::string> offense = collectBattleCards(targetPlayer, true);
	return anyMatchingCard(offense, voice.target);
}

bool voiceAffectsOffense(BeneVoiceTarget target) {
	return target == BeneVoiceTarget::WEAPON_POISON ||
		target == BeneVoiceTarget::WEAPON_PROJECTILE ||
		target == BeneVoiceTarget::WEAPON_LASGUN ||
		target == BeneVoiceTarget::WORTHLESS;
}

bool voiceAffectsDefense(BeneVoiceTarget target) {
	return target == BeneVoiceTarget::DEFENSE_SHIELD ||
		target == BeneVoiceTarget::DEFENSE_SNOOPER ||
		target == BeneVoiceTarget::WORTHLESS;
}

std::vector<std::string> applyVoiceConstraintToSlot(const std::vector<std::string>& cards,
	const BeneVoiceState& voice, bool offenseSlot, bool& constraintApplied) {
	constraintApplied = false;
	const bool affectsSlot = offenseSlot ? voiceAffectsOffense(voice.target) : voiceAffectsDefense(voice.target);
	if (!affectsSlot) {
		return cards;
	}

	std::vector<std::string> filtered;
	for (const std::string& c : cards) {
		bool match = cardMatchesVoiceTarget(c, voice.target);
		if ((voice.demandPlay && match) || (!voice.demandPlay && !match)) {
			filtered.push_back(c);
		}
	}

	if (!filtered.empty() || !voice.demandPlay) {
		constraintApplied = true;
		return filtered;
	}

	return cards;
}

BeneVoiceState prepareBeneVoiceForBattle(PhaseContext& ctx, int attackerIdx, int defenderIdx,
	Player* attacker, Player* defender) {
	BeneVoiceState voice;

	int beneIdx = -1;
	int targetIdx = -1;
	Player* benePlayer = nullptr;
	Player* targetPlayer = nullptr;

	if (attacker->getFactionName() == "Bene Gesserit") {
		beneIdx = attackerIdx;
		targetIdx = defenderIdx;
		benePlayer = attacker;
		targetPlayer = defender;
	} else if (defender->getFactionName() == "Bene Gesserit") {
		beneIdx = defenderIdx;
		targetIdx = attackerIdx;
		benePlayer = defender;
		targetPlayer = attacker;
	} else {
		return voice;
	}

	voice.active = true;
	voice.beneIdx = beneIdx;
	voice.targetIdx = targetIdx;

	if (!ctx.adapter) {
		voice.active = false;
		return voice;
	}

	{
		DecisionRequest req;
		req.kind = "yn";
		req.actor_index = voice.beneIdx;
		req.prompt = "[Bene Voice] Use Voice against " + targetPlayer->getFactionName() + " this battle?";
		auto resp = ctx.adapter->requestDecision(req);
		if (!resp || !resp->valid || resp->payload_json != "y") {
			voice.active = false;
			return voice;
		}
	}

	static const std::vector<std::string> voiceOptions = {
		"PLAY poison weapon", "DON'T PLAY poison weapon",
		"PLAY projectile weapon", "DON'T PLAY projectile weapon",
		"PLAY lasgun", "DON'T PLAY lasgun",
		"PLAY shield", "DON'T PLAY shield",
		"PLAY snooper", "DON'T PLAY snooper",
		"PLAY worthless card", "DON'T PLAY worthless card",
		"PLAY cheap hero", "DON'T PLAY cheap hero"
	};

	int cmd = 0;
	{
		DecisionRequest req;
		req.kind = "select";
		req.actor_index = voice.beneIdx;
		req.prompt = "[Bene Voice] Command type:";
		req.options = voiceOptions;
		req.allow_none = false;
		auto resp = ctx.adapter->requestDecision(req);
		if (resp && resp->valid) {
			for (int i = 0; i < (int)voiceOptions.size(); ++i) {
				if (voiceOptions[i] == resp->payload_json) { cmd = i + 1; break; }
			}
		}
	}
	if (cmd == 0) { voice.active = false; return voice; }

	voice.demandPlay = (cmd % 2 == 1);
	switch (cmd) {
		case 1: case 2: voice.target = BeneVoiceTarget::WEAPON_POISON; break;
		case 3: case 4: voice.target = BeneVoiceTarget::WEAPON_PROJECTILE; break;
		case 5: case 6: voice.target = BeneVoiceTarget::WEAPON_LASGUN; break;
		case 7: case 8: voice.target = BeneVoiceTarget::DEFENSE_SHIELD; break;
		case 9: case 10: voice.target = BeneVoiceTarget::DEFENSE_SNOOPER; break;
		case 11: case 12: voice.target = BeneVoiceTarget::WORTHLESS; break;
		case 13: case 14: voice.target = BeneVoiceTarget::CHEAP_HERO; break;
	}

	if (!canComplyWithBeneVoice(targetPlayer, voice) && voice.demandPlay) {
		if (ctx.logger) {
			ctx.logger->logDebug("[Bene Voice] Opponent cannot comply with command \"PLAY " +
				beneVoiceTargetToString(voice.target) + "\" and may play freely.");
		}
		voice.active = false;
		return voice;
	}

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Voice] Command issued: " + std::string(voice.demandPlay ? "PLAY " : "DON'T PLAY ") +
			beneVoiceTargetToString(voice.target));
	}

	(void)benePlayer;
	return voice;
}

std::string battleElementToString(BattleElementToPeek element) {
	switch (element) {
		case BattleElementToPeek::LEADER: return "leader";
		case BattleElementToPeek::WEAPON: return "weapon";
		case BattleElementToPeek::DEFENSE: return "defense";
		case BattleElementToPeek::DIAL: return "dial";
	}
	return "leader";
}

BattleElementToPeek askAtreidesWhichElementToPeek(PhaseContext& ctx, const Player* atreides, const Player* opponent) {
	BattleElementToPeek choice = BattleElementToPeek::LEADER;

	if (ctx.logger) {
		ctx.logger->logDebug("[Atreides Prescience] " + atreides->getFactionName() +
			" asks " + opponent->getFactionName() + " about one Battle Plan element.");
	}

	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "select";
		req.prompt = "[Atreides Prescience] Choose which element to reveal from opponent:";
		req.options = {"Leader", "Weapon", "Defense", "Dial (wheel value)"};
		req.allow_none = false;
		auto resp = ctx.adapter->requestDecision(req);
		if (resp && resp->valid) {
			if      (resp->payload_json == "Leader")            choice = BattleElementToPeek::LEADER;
			else if (resp->payload_json == "Weapon")            choice = BattleElementToPeek::WEAPON;
			else if (resp->payload_json == "Defense")           choice = BattleElementToPeek::DEFENSE;
			else if (resp->payload_json == "Dial (wheel value)") choice = BattleElementToPeek::DIAL;
		}
	}

	return choice;
}

AtreidesPeekState prepareAtreidesPeekForBattle(PhaseContext& ctx, int attackerIdx, int defenderIdx,
	Player* attacker, Player* defender) {
	AtreidesPeekState state;
	if (attacker->getFactionName() == "Atreides") {
		state.active = true;
		state.atreidesIdx = attackerIdx;
		state.opponentIdx = defenderIdx;
		state.element = askAtreidesWhichElementToPeek(ctx, attacker, defender);
	} else if (defender->getFactionName() == "Atreides") {
		state.active = true;
		state.atreidesIdx = defenderIdx;
		state.opponentIdx = attackerIdx;
		state.element = askAtreidesWhichElementToPeek(ctx, defender, attacker);
	}

	if (state.active && ctx.logger) {
		ctx.logger->logDebug("[Atreides Prescience] Locked question: " + battleElementToString(state.element));
	}

	return state;
}

bool lockOpponentElementForAtreides(PhaseContext& ctx, const AtreidesPeekState& peek,
	Player* opponent,
	LockedBattleElement& locked,
	BeneVoiceState& voice) {
	if (!peek.active) {
		return true;
	}

	locked.active = true;
	locked.element = peek.element;

	if (ctx.logger) {
		ctx.logger->logDebug("[Atreides Prescience] Opponent must now reveal and lock: " +
			battleElementToString(peek.element));
	}

	switch (peek.element) {
		case BattleElementToPeek::LEADER: {
			const bool voiceApplies = voice.active && peek.opponentIdx == voice.targetIdx &&
				voice.target == BeneVoiceTarget::CHEAP_HERO;
			BattleLeaderChoice choice = selectBattleLeaderChoice(ctx, opponent,
				voiceApplies && voice.demandPlay,
				voiceApplies && !voice.demandPlay);
			if (!choice.valid) {
				return false;
			}
			locked.leaderChoice = choice;
			locked.hasLeader = true;
			if (!choice.usedCheapHero && choice.aliveLeaderIndex >= 0) {
				opponent->markLeaderBattled(static_cast<size_t>(choice.aliveLeaderIndex));
			}
			if (ctx.logger) {
				ctx.logger->logDebug("[Atreides Prescience] Locked leader: " + choice.leaderName +
					" (power: " + std::to_string(choice.leaderPower) + ")");
			}
			break;
		}
		case BattleElementToPeek::WEAPON: {
			std::vector<std::string> offenseChoices = collectBattleCards(opponent, true);
			if (voice.active && peek.opponentIdx == voice.targetIdx) {
				bool constraintApplied = false;
				offenseChoices = applyVoiceConstraintToSlot(offenseChoices, voice, true, constraintApplied);
			}
			locked.weapon = selectBattleCard(ctx, opponent, offenseChoices, "offense (locked)");
			locked.hasWeapon = true;
			if (voice.active && voice.demandPlay && voice.target == BeneVoiceTarget::WORTHLESS &&
				peek.opponentIdx == voice.targetIdx && isWorthlessCard(locked.weapon)) {
				voice.worthlessSatisfied = true;
			}
			if (ctx.logger) {
				ctx.logger->logDebug("[Atreides Prescience] Locked weapon: " +
					(locked.weapon.empty() ? "None" : locked.weapon));
			}
			break;
		}
		case BattleElementToPeek::DEFENSE: {
			std::vector<std::string> defenseChoices = collectBattleCards(opponent, false);
			if (voice.active && peek.opponentIdx == voice.targetIdx) {
				bool constraintApplied = false;
				defenseChoices = applyVoiceConstraintToSlot(defenseChoices, voice, false, constraintApplied);
			}
			locked.defense = selectBattleCard(ctx, opponent, defenseChoices, "defense (locked)");
			locked.hasDefense = true;
			if (voice.active && voice.demandPlay && voice.target == BeneVoiceTarget::WORTHLESS &&
				peek.opponentIdx == voice.targetIdx && isWorthlessCard(locked.defense)) {
				voice.worthlessSatisfied = true;
			}
			if (ctx.logger) {
				ctx.logger->logDebug("[Atreides Prescience] Locked defense: " +
					(locked.defense.empty() ? "None" : locked.defense));
			}
			break;
		}
		case BattleElementToPeek::DIAL: {
			if (ctx.logger) {
				ctx.logger->logDebug("[Atreides Prescience] Dial will be revealed and locked now.");
			}
			break;
		}
	}

	return true;
}

}

BattlePhase::BattlePhase() {
}

void BattlePhase::execute(PhaseContext& ctx) {
	if (ctx.logger) {
		ctx.logger->logDebug("BATTLE Phase");
	}

	// Get view for this phase
	auto view = ctx.getBattleView();

	// Battle resolution follows turn order
	// Each player's turn: resolve all their contested battles
	const auto& turnOrder = view.turnOrder;
	int totalBattles = 0;

	for (int playerIdx : turnOrder) {
		Player* attacker = view.players[playerIdx];
		
		// Find all contested territories where this player has units
		std::vector<std::string> contestedTerritories;
		const auto& territories = view.map.getTerritories();
		
		for (const auto& terr : territories) {
			// Skip Polar Sink (no battles)
			if (terr.name == "Polar Sink") continue;
			
			// Check if this player has units here
			int playerUnits = view.map.getCombatUnitsInTerritory(terr.name, playerIdx);
			if (playerUnits == 0) continue;
			
			// Check if contested (other players also have units)
			bool contested = false;
			for (size_t i = 0; i < view.players.size(); ++i) {
				if (i != (size_t)playerIdx) {
					int otherUnits = view.map.getCombatUnitsInTerritory(terr.name, i);
					if (otherUnits > 0) {
						contested = true;
						break;
					}
				}
			}
			
			if (contested) {
				contestedTerritories.push_back(terr.name);
			}
		}

		// Player resolves battles in contested territories
		while (!contestedTerritories.empty()) {
			if (ctx.adapter && contestedTerritories.size() > 1) {
				DecisionRequest req;
				req.kind = "select";
				req.actor_index = playerIdx;
				req.prompt = attacker->getFactionName() + "'s contested territories — choose which to battle first:";
				req.options = contestedTerritories;
				req.allow_none = false;
				auto resp = ctx.adapter->requestDecision(req);
				std::string chosen = contestedTerritories[0];
				if (resp && resp->valid && !resp->payload_json.empty()) {
					chosen = resp->payload_json;
				}
				int choice = 0;
				for (int i = 0; i < (int)contestedTerritories.size(); ++i) {
					if (contestedTerritories[i] == chosen) { choice = i; break; }
				}

				resolveBattle(ctx, playerIdx, contestedTerritories[choice]);
				totalBattles++;
				contestedTerritories.erase(contestedTerritories.begin() + choice);
			} else if (!contestedTerritories.empty()) {
				// Only one contested territory or non-interactive mode
				resolveBattle(ctx, playerIdx, contestedTerritories[0]);
				totalBattles++;
				contestedTerritories.erase(contestedTerritories.begin());
			}
		}
	}

	if (ctx.logger) {
		if (totalBattles == 0) {
			ctx.logger->logDebug("No battles occurred this turn.");
		} else {
			ctx.logger->logDebug("=== " + std::to_string(totalBattles) + " battle(s) resolved ===");
		}
	}
}

void BattlePhase::resolveBattle(PhaseContext& ctx, int attackerIdx, const std::string& territoryName) {
	auto view = ctx.getBattleView();
	Player* attacker = view.players[attackerIdx];
	
	// Find all defenders in this territory
	std::vector<int> defenderIndices;
	for (size_t i = 0; i < view.players.size(); ++i) {
		if (i != (size_t)attackerIdx) {
			int units = view.map.getCombatUnitsInTerritory(territoryName, i);
			if (units > 0) {
				defenderIndices.push_back(i);
			}
		}
	}

	if (defenderIndices.empty()) return; // No defenders

	if (ctx.logger) {
		std::string defendersStr;
		for (size_t i = 0; i < defenderIndices.size(); ++i) {
			defendersStr += view.players[defenderIndices[i]]->getFactionName();
			if (i < defenderIndices.size() - 1) defendersStr += ", ";
		}
		ctx.logger->logDebug("=== Battle in " + territoryName + " ===");
		ctx.logger->logDebug(attacker->getFactionName() + " (attacker) vs " + defendersStr);
	}

	// Attacker must have either a real leader or a Cheap Hero.
	if (attacker->getAliveLeaders().empty() && !hasCheapHeroCard(attacker)) {
		if (ctx.logger) {
			ctx.logger->logDebug(attacker->getFactionName() + " has no leader and no Cheap Hero. Battle cannot occur.");
		}
		return;
	}

	// For now, pick first defender (in multi-defender scenarios, could be more complex)
	int defenderIdx = defenderIndices[0];
	Player* defender = view.players[defenderIdx];

	const std::string attackerFaction = attacker->getFactionName();
	const std::string defenderFaction = defender->getFactionName();
	int attackerEliteStrength = 1;
	int defenderEliteStrength = 1;
	if (ctx.featureSettings.advancedFactionAbilities) {
		attackerEliteStrength =
			(attackerFaction == "Emperor" && defenderFaction == "Fremen") ? 1 : 2;
		defenderEliteStrength =
			(defenderFaction == "Emperor" && attackerFaction == "Fremen") ? 1 : 2;
	}
	// Precedence when both are present: Bene Voice first, then Atreides question, then normal flow.
	BeneVoiceState beneVoice = prepareBeneVoiceForBattle(ctx, attackerIdx, defenderIdx, attacker, defender);
	AtreidesPeekState atreidesPeek =
		prepareAtreidesPeekForBattle(ctx, attackerIdx, defenderIdx, attacker, defender);
	LockedBattleElement lockedElement;
	if (atreidesPeek.active) {
		Player* opponent = view.players[atreidesPeek.opponentIdx];
		const int opponentEliteStrength =
			(atreidesPeek.opponentIdx == attackerIdx) ? attackerEliteStrength : defenderEliteStrength;
		if (!lockOpponentElementForAtreides(ctx, atreidesPeek, opponent, lockedElement, beneVoice)) {
			return;
		}
		if (lockedElement.active && lockedElement.element == BattleElementToPeek::DIAL) {
			int units = view.map.getCombatUnitsInTerritory(territoryName, atreidesPeek.opponentIdx);
			lockedElement.dialValue = getBattleWheelChoice(ctx, atreidesPeek.opponentIdx, units,
				territoryName, opponentEliteStrength);
			lockedElement.hasDial = true;
			if (ctx.logger) {
				ctx.logger->logDebug("[Atreides Prescience] Locked dial: " + std::to_string(lockedElement.dialValue));
			}
		}
	}

	const auto& defenderLeaders = defender->getAliveLeaders();

	if (defenderLeaders.empty() && !hasCheapHeroCard(defender)) {
		if (ctx.logger) {
			ctx.logger->logDebug(defender->getFactionName() + " has no alive leaders. "
		          + attacker->getFactionName() + " wins by default.");
		}

		BattleLeaderChoice attackerChoice = selectBattleLeaderChoice(ctx, attacker);
		if (!attackerChoice.valid) return;

		// Mark attacker leader as battled and snapshot for this battle.
		if (!attackerChoice.usedCheapHero && attackerChoice.aliveLeaderIndex >= 0) {
			attacker->markLeaderBattled(static_cast<size_t>(attackerChoice.aliveLeaderIndex));
		}
		int attackerLeaderPower = attackerChoice.leaderPower;

		// Attacker survives with remaining strength after dialing.
		int attackerUnits = view.map.getCombatUnitsInTerritory(territoryName, attackerIdx);
		int wheelValue = getBattleWheelChoice(ctx, attackerIdx, attackerUnits, territoryName, attackerEliteStrength);
		auto [normalUnits, eliteUnits] = view.map.getCombatUnitBreakdown(territoryName, attackerIdx);
		int unitStrength = calculateUnitStrength(wheelValue, normalUnits, eliteUnits, attackerEliteStrength);

		if (ctx.logger) {
			ctx.logger->logDebug(attacker->getFactionName() + " battle value: " + std::to_string(unitStrength)
				+ " (units: " + std::to_string(wheelValue) + " committed from " + std::to_string(normalUnits)
				+ "N+" + std::to_string(eliteUnits) + "E) + " + std::to_string(attackerLeaderPower)
				+ " (leader) = " + std::to_string(unitStrength + attackerLeaderPower));
		}

		auto [aN, aE] = view.map.getCombatUnitBreakdown(territoryName, attackerIdx);
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses dialed strength worth of units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		
		// Get defender's breakdown for full removal
		auto [defN, defE] = view.map.getCombatUnitBreakdown(territoryName, defenderIdx);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				attacker->getFactionName() + " wins in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}

		if (attackerChoice.usedCheapHero) {
			attacker->removeTreacheryCard("Cheap Hero");
			if (ctx.logger) {
				ctx.logger->logDebug(attacker->getFactionName() + " discards \"Cheap Hero\" after battle.");
			}
		}
		return;
	}

	BattleLeaderChoice defenderChoice;
	BattleLeaderChoice attackerChoice;
	int defenderLeaderPower = 0;
	int attackerLeaderPower = 0;
	std::string defenderWeapon;
	std::string defenderDefense;
	std::string attackerWeapon;
	std::string attackerDefense;
	int defWheelValue = 0;
	int wheelValue = 0;

	auto commitPlanForPlayer = [&](int playerIdx, Player* player, int eliteStrength,
		BattleLeaderChoice& outChoice, int& outLeaderPower, std::string& outWeapon,
		std::string& outDefense, int& outDialValue) -> bool {
		if (ctx.logger) {
			ctx.logger->logDebug("--- Battle Plan: " + player->getFactionName() + " ---");
		}

		const bool thisIsLockedOpponent = (lockedElement.active && playerIdx == atreidesPeek.opponentIdx);
		const bool thisIsVoiceTarget = (beneVoice.active && playerIdx == beneVoice.targetIdx);

		if (thisIsLockedOpponent && lockedElement.hasLeader) {
			outChoice = lockedElement.leaderChoice;
		} else {
			bool forceCheapHero = false;
			bool forbidCheapHero = false;
			if (thisIsVoiceTarget && beneVoice.target == BeneVoiceTarget::CHEAP_HERO) {
				forceCheapHero = beneVoice.demandPlay;
				forbidCheapHero = !beneVoice.demandPlay;
			}
			outChoice = selectBattleLeaderChoice(ctx, player, forceCheapHero, forbidCheapHero);
			if (!outChoice.valid && forceCheapHero) {
				// Cannot comply with "play cheap hero" -> may do as they wish.
				outChoice = selectBattleLeaderChoice(ctx, player);
			}
			if (!outChoice.valid) return false;
			if (!outChoice.usedCheapHero && outChoice.aliveLeaderIndex >= 0) {
				player->markLeaderBattled(static_cast<size_t>(outChoice.aliveLeaderIndex));
			}
		}
		outLeaderPower = outChoice.leaderPower;

		if (thisIsLockedOpponent && lockedElement.hasWeapon) {
			outWeapon = lockedElement.weapon;
			if (thisIsVoiceTarget && beneVoice.demandPlay && beneVoice.target == BeneVoiceTarget::WORTHLESS &&
				isWorthlessCard(outWeapon)) {
				beneVoice.worthlessSatisfied = true;
			}
		} else {
			std::vector<std::string> offenseChoices = collectBattleCards(player, true);
			if (thisIsLockedOpponent && lockedElement.hasDefense) {
				removeOneCardInstance(offenseChoices, lockedElement.defense);
			}
			bool offenseAllowNone = true;
			if (thisIsVoiceTarget) {
				bool constraintApplied = false;
				offenseChoices = applyVoiceConstraintToSlot(offenseChoices, beneVoice, true, constraintApplied);
				if (constraintApplied && beneVoice.demandPlay) {
					offenseAllowNone = false;
				}
			}
			outWeapon = selectBattleCard(ctx, player, offenseChoices, "offense", offenseAllowNone);
			if (thisIsVoiceTarget && beneVoice.demandPlay && beneVoice.target == BeneVoiceTarget::WORTHLESS &&
				isWorthlessCard(outWeapon)) {
				beneVoice.worthlessSatisfied = true;
			}
		}

		if (thisIsLockedOpponent && lockedElement.hasDefense) {
			outDefense = lockedElement.defense;
			if (thisIsVoiceTarget && beneVoice.demandPlay && beneVoice.target == BeneVoiceTarget::WORTHLESS &&
				isWorthlessCard(outDefense)) {
				beneVoice.worthlessSatisfied = true;
			}
		} else {
			std::vector<std::string> defenseChoices = collectBattleCards(player, false);
			removeOneCardInstance(defenseChoices, outWeapon);
			bool defenseAllowNone = true;
			if (thisIsVoiceTarget) {
				bool constraintApplied = false;
				defenseChoices = applyVoiceConstraintToSlot(defenseChoices, beneVoice, false, constraintApplied);
				if (constraintApplied && beneVoice.demandPlay) {
					defenseAllowNone = false;
				}

				// Worthless command may be satisfied by either slot; if not yet satisfied and possible here, force it.
				if (beneVoice.target == BeneVoiceTarget::WORTHLESS && beneVoice.demandPlay &&
					!beneVoice.worthlessSatisfied) {
					std::vector<std::string> worthlessOnly;
					for (const std::string& c : defenseChoices) {
						if (isWorthlessCard(c)) worthlessOnly.push_back(c);
					}
					if (!worthlessOnly.empty()) {
						defenseChoices = worthlessOnly;
						defenseAllowNone = false;
					}
				}
			}
			outDefense = selectBattleCard(ctx, player, defenseChoices, "defense", defenseAllowNone);
			if (thisIsVoiceTarget && beneVoice.demandPlay && beneVoice.target == BeneVoiceTarget::WORTHLESS &&
				isWorthlessCard(outDefense)) {
				beneVoice.worthlessSatisfied = true;
			}
		}

		if (thisIsLockedOpponent && lockedElement.hasDial) {
			outDialValue = lockedElement.dialValue;
		} else {
			int units = view.map.getCombatUnitsInTerritory(territoryName, playerIdx);
			outDialValue = getBattleWheelChoice(ctx, playerIdx, units, territoryName, eliteStrength);
		}

		return true;
	};

	if (atreidesPeek.active) {
		Player* atreidesPlayer = view.players[atreidesPeek.atreidesIdx];
		Player* opponentPlayer = view.players[atreidesPeek.opponentIdx];
		const int atreidesEliteStrength =
			(atreidesPeek.atreidesIdx == attackerIdx) ? attackerEliteStrength : defenderEliteStrength;
		const int opponentEliteStrength =
			(atreidesPeek.opponentIdx == attackerIdx) ? attackerEliteStrength : defenderEliteStrength;

		if (!commitPlanForPlayer(atreidesPeek.atreidesIdx, atreidesPlayer, atreidesEliteStrength,
			(atreidesPeek.atreidesIdx == attackerIdx ? attackerChoice : defenderChoice),
			(atreidesPeek.atreidesIdx == attackerIdx ? attackerLeaderPower : defenderLeaderPower),
			(atreidesPeek.atreidesIdx == attackerIdx ? attackerWeapon : defenderWeapon),
			(atreidesPeek.atreidesIdx == attackerIdx ? attackerDefense : defenderDefense),
			(atreidesPeek.atreidesIdx == attackerIdx ? wheelValue : defWheelValue))) {
			return;
		}

		if (!commitPlanForPlayer(atreidesPeek.opponentIdx, opponentPlayer, opponentEliteStrength,
			(atreidesPeek.opponentIdx == attackerIdx ? attackerChoice : defenderChoice),
			(atreidesPeek.opponentIdx == attackerIdx ? attackerLeaderPower : defenderLeaderPower),
			(atreidesPeek.opponentIdx == attackerIdx ? attackerWeapon : defenderWeapon),
			(atreidesPeek.opponentIdx == attackerIdx ? attackerDefense : defenderDefense),
			(atreidesPeek.opponentIdx == attackerIdx ? wheelValue : defWheelValue))) {
			return;
		}
	} else {
		if (!commitPlanForPlayer(defenderIdx, defender, defenderEliteStrength,
			defenderChoice, defenderLeaderPower, defenderWeapon, defenderDefense, defWheelValue)) {
			return;
		}
		if (!commitPlanForPlayer(attackerIdx, attacker, attackerEliteStrength,
			attackerChoice, attackerLeaderPower, attackerWeapon, attackerDefense, wheelValue)) {
			return;
		}
	}


	if (ctx.logger) {
		ctx.logger->logDebug(attacker->getFactionName() + " battle cards: weapon=" +
			(attackerWeapon.empty() ? "None" : attackerWeapon) + ", defense=" +
			(attackerDefense.empty() ? "None" : attackerDefense));
		ctx.logger->logDebug(defender->getFactionName() + " battle cards: weapon=" +
			(defenderWeapon.empty() ? "None" : defenderWeapon) + ", defense=" +
			(defenderDefense.empty() ? "None" : defenderDefense));
	}

	const bool lasgunShieldExplosion =
		(attackerWeapon == "Lasgun" && defenderDefense == "Shield") ||
		(defenderWeapon == "Lasgun" && attackerDefense == "Shield");
	if (lasgunShieldExplosion) {
		if (ctx.logger) {
			ctx.logger->logDebug("Lasgun/Shield explosion! All forces, leaders, and spice in " +
				territoryName + " are destroyed. Both players lose battle.");
		}

		territory* terr = view.map.getTerritory(territoryName);
		if (terr) {
			for (const auto& stack : terr->unitsPresent) {
				if (stack.normal_units > 0) {
					ctx.players[stack.factionOwner]->destroyUnits(stack.normal_units);
				}
				if (stack.elite_units > 0) {
					ctx.players[stack.factionOwner]->destroyEliteUnits(stack.elite_units);
				}
			}
			terr->unitsPresent.clear();
			view.map.removeAllSpiceFromTerritory(territoryName);
		}

		if (!attackerChoice.usedCheapHero) {
			killLeaderByName(attacker, attackerChoice.leaderName);
		}
		if (!defenderChoice.usedCheapHero) {
			killLeaderByName(defender, defenderChoice.leaderName);
		}

		if (!attackerWeapon.empty()) attacker->removeTreacheryCard(attackerWeapon);
		if (!attackerDefense.empty()) attacker->removeTreacheryCard(attackerDefense);
		if (!defenderWeapon.empty()) defender->removeTreacheryCard(defenderWeapon);
		if (!defenderDefense.empty()) defender->removeTreacheryCard(defenderDefense);
		if (attackerChoice.usedCheapHero) attacker->removeTreacheryCard("Cheap Hero");
		if (defenderChoice.usedCheapHero) defender->removeTreacheryCard("Cheap Hero");

		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"Lasgun/Shield explosion in " + territoryName + ": both battle plans destroyed",
				ctx.turnNumber, "BATTLE");
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		return;
	}

	// Weapon effects happen before battle values are resolved.
	const bool attackerLeaderKilled = weaponKillsLeader(defenderWeapon, attackerDefense);
	const bool defenderLeaderKilled = weaponKillsLeader(attackerWeapon, defenderDefense);

	if (attackerLeaderKilled) {
		attackerLeaderPower = 0;
		if (!attackerChoice.usedCheapHero) {
			killLeaderByName(attacker, attackerChoice.leaderName);
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Weapon effect: " + attacker->getFactionName() + " leader " +
				attackerChoice.leaderName + " is killed before battle resolution.");
		}
	} else if (!defenderWeapon.empty() && !attackerDefense.empty() && ctx.logger) {
		ctx.logger->logDebug("Defense saved " + attacker->getFactionName() + " leader from " + defenderWeapon + ".");
	}

	if (defenderLeaderKilled) {
		defenderLeaderPower = 0;
		if (!defenderChoice.usedCheapHero) {
			killLeaderByName(defender, defenderChoice.leaderName);
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Weapon effect: " + defender->getFactionName() + " leader " +
				defenderChoice.leaderName + " is killed before battle resolution.");
		}
	} else if (!attackerWeapon.empty() && !defenderDefense.empty() && ctx.logger) {
		ctx.logger->logDebug("Defense saved " + defender->getFactionName() + " leader from " + attackerWeapon + ".");
	}

	// Calculate actual unit strength (elite units count as 2x unless faction matchup modifies it)
	int attackerUnits = view.map.getCombatUnitsInTerritory(territoryName, attackerIdx);
	int defenderUnits = view.map.getCombatUnitsInTerritory(territoryName, defenderIdx);
	auto attackerBreakdown = view.map.getCombatUnitBreakdown(territoryName, attackerIdx);
	auto defenderBreakdown = view.map.getCombatUnitBreakdown(territoryName, defenderIdx);
	int normalUnits = attackerBreakdown.first;
	int eliteUnits = attackerBreakdown.second;
	int defNormalUnits = defenderBreakdown.first;
	int defEliteUnits = defenderBreakdown.second;
	int unitStrength = calculateUnitStrength(wheelValue, normalUnits, eliteUnits, attackerEliteStrength);
	int defUnitStrength = calculateUnitStrength(defWheelValue, defNormalUnits, defEliteUnits, defenderEliteStrength);

	// Leader contributes 0 power if killed by weapon this battle.
	int attackerValue = unitStrength + attackerLeaderPower;
	int defenderValue = defUnitStrength + defenderLeaderPower;
	if (ctx.logger) {
		ctx.logger->logDebug(attacker->getFactionName() + " battle value: " + std::to_string(unitStrength)
			+ " (units: " + std::to_string(wheelValue) + " committed from " + std::to_string(normalUnits)
			+ "N+" + std::to_string(eliteUnits) + "E) + " + std::to_string(attackerLeaderPower)
			+ " (leader) = " + std::to_string(attackerValue));
		ctx.logger->logDebug(defender->getFactionName() + " battle value: " + std::to_string(defUnitStrength)
			+ " (units: " + std::to_string(defWheelValue) + " committed from " + std::to_string(defNormalUnits)
			+ "N+" + std::to_string(defEliteUnits) + "E) + " + std::to_string(defenderLeaderPower)
			+ " (leader) = " + std::to_string(defenderValue));
	}

	auto resolveBattleCardAftermath = [&](int winnerIdx, int loserIdx) {
		Player* winner = view.players[winnerIdx];
		Player* loser = view.players[loserIdx];

		if (!attackerWeapon.empty() && attackerIdx == loserIdx) loser->removeTreacheryCard(attackerWeapon);
		if (!attackerDefense.empty() && attackerIdx == loserIdx) loser->removeTreacheryCard(attackerDefense);
		if (!defenderWeapon.empty() && defenderIdx == loserIdx) loser->removeTreacheryCard(defenderWeapon);
		if (!defenderDefense.empty() && defenderIdx == loserIdx) loser->removeTreacheryCard(defenderDefense);

		if (ctx.logger) {
			ctx.logger->logDebug(loser->getFactionName() + " discards battle cards used this combat.");
		}

		auto maybeKeep = [&](const std::string& cardName) {
			if (cardName.empty()) return;

			if (isWorthlessCard(cardName)) {
				winner->removeTreacheryCard(cardName);
				if (ctx.logger) {
					ctx.logger->logDebug(winner->getFactionName() + " discards worthless card \"" + cardName + "\".");
				}
				return;
			}

			bool keep = true;
			if (ctx.adapter) {
				DecisionRequest req;
				req.kind = "yn";
				req.prompt = winner->getFactionName() + ", keep card \"" + cardName + "\"?";
				auto resp = ctx.adapter->requestDecision(req);
				keep = resp && resp->valid && resp->payload_json == "y";
			}

			if (!keep) {
				winner->removeTreacheryCard(cardName);
				if (ctx.logger) {
					ctx.logger->logDebug(winner->getFactionName() + " discards \"" + cardName + "\".");
				}
			} else if (ctx.logger) {
				ctx.logger->logDebug(winner->getFactionName() + " keeps \"" + cardName + "\".");
			}
		};

		if (winnerIdx == attackerIdx) {
			maybeKeep(attackerWeapon);
			maybeKeep(attackerDefense);
		} else {
			maybeKeep(defenderWeapon);
			maybeKeep(defenderDefense);
		}

		if (attackerChoice.usedCheapHero) {
			attacker->removeTreacheryCard("Cheap Hero");
			if (ctx.logger) {
				ctx.logger->logDebug(attacker->getFactionName() + " discards \"Cheap Hero\" after battle.");
			}
		}
		if (defenderChoice.usedCheapHero) {
			defender->removeTreacheryCard("Cheap Hero");
			if (ctx.logger) {
				ctx.logger->logDebug(defender->getFactionName() + " discards \"Cheap Hero\" after battle.");
			}
		}
	};

	auto awardWinnerSpiceForDeadLeaders = [&](Player* winner) {
		int spiceAward = 0;
		if (attackerLeaderKilled) spiceAward += attackerChoice.leaderPower;
		if (defenderLeaderKilled) spiceAward += defenderChoice.leaderPower;
		if (spiceAward <= 0) return;

		winner->addSpice(spiceAward);
		if (ctx.logger) {
			ctx.logger->logDebug("Battle reward: " + winner->getFactionName() +
				" gains " + std::to_string(spiceAward) + " spice for dead leader(s) in this battle.");
		}
	};

	int winnerIdx = attackerIdx;
	int loserIdx = defenderIdx;
	bool winnerIsAttacker = true;
	bool isTie = false;

	// Determine winner (attacker wins ties)
	if (attackerValue > defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + attacker->getFactionName() + " wins!");
		}
	} else if (attackerValue < defenderValue) {
		winnerIdx = defenderIdx;
		loserIdx = attackerIdx;
		winnerIsAttacker = false;
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + defender->getFactionName() + " wins!");
		}
	} else {
		isTie = true;
		if (ctx.logger) {
			ctx.logger->logDebug("Result: Tie! " + attacker->getFactionName() + " wins (attacker advantage).");
		}
	}

	BattleOutcomeInputs outcomeInput;
	outcomeInput.attackerIdx = attackerIdx;
	outcomeInput.defenderIdx = defenderIdx;
	outcomeInput.territoryName = territoryName;
	outcomeInput.winnerIdx = winnerIdx;
	outcomeInput.loserIdx = loserIdx;
	outcomeInput.winnerIsAttacker = winnerIsAttacker;
	outcomeInput.isTie = isTie;
	outcomeInput.attackerUnits = attackerUnits;
	outcomeInput.defenderUnits = defenderUnits;
	outcomeInput.normalUnits = normalUnits;
	outcomeInput.eliteUnits = eliteUnits;
	outcomeInput.defNormalUnits = defNormalUnits;
	outcomeInput.defEliteUnits = defEliteUnits;
	outcomeInput.unitStrength = unitStrength;
	outcomeInput.defUnitStrength = defUnitStrength;
	outcomeInput.attackerEliteStrength = attackerEliteStrength;
	outcomeInput.defenderEliteStrength = defenderEliteStrength;
	applyBattleOutcome(ctx, outcomeInput);

	Player* winner = view.players[winnerIdx];
	if (ctx.featureSettings.advancedFactionAbilities) {
		winner->getFactionAbility()->onBattleWon(ctx, loserIdx);
	}
	awardWinnerSpiceForDeadLeaders(winner);
	resolveBattleCardAftermath(winnerIdx, loserIdx);
}

void BattlePhase::applyBattleOutcome(PhaseContext& ctx, const BattleOutcomeInputs& input) {
	auto view = ctx.getBattleView();
	auto [aN, aE] = view.map.getCombatUnitBreakdown(input.territoryName, input.attackerIdx);
	auto [defN, defE] = view.map.getCombatUnitBreakdown(input.territoryName, input.defenderIdx);

	if (input.winnerIsAttacker) {
		int totalAttackerStrength = calculateUnitStrength(input.attackerUnits, input.normalUnits,
			input.eliteUnits, input.attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - input.unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, input.attackerIdx,
			attackerRemainingStrength, aN, aE, input.attackerEliteStrength);

		view.map.removeUnitsFromTerritory(input.territoryName, input.attackerIdx, aNKilled, aEKilled);
		view.map.removeUnitsFromTerritory(input.territoryName, input.defenderIdx, defN, defE);
	} else {
		int totalDefenderStrength = calculateUnitStrength(input.defenderUnits, input.defNormalUnits,
			input.defEliteUnits, input.defenderEliteStrength);
		int defenderRemainingStrength = std::max(0, totalDefenderStrength - input.defUnitStrength);
		auto [defNKilled, defEKilled] = askCasualtyDistribution(ctx, input.defenderIdx,
			defenderRemainingStrength, defN, defE, input.defenderEliteStrength);

		view.map.removeUnitsFromTerritory(input.territoryName, input.attackerIdx, aN, aE);
		view.map.removeUnitsFromTerritory(input.territoryName, input.defenderIdx, defNKilled, defEKilled);
	}

	if (ctx.logger) {
		Player* winner = view.players[input.winnerIdx];
		Player* loser = view.players[input.loserIdx];
		Player* attacker = view.players[input.attackerIdx];
		std::string msg = input.isTie
			? ("Tie in " + input.territoryName + ": " + attacker->getFactionName() + " wins")
			: (winner->getFactionName() + " defeats " + loser->getFactionName() + " in " + input.territoryName);
		Event e(EventType::BATTLE_RESOLVED, msg, ctx.turnNumber, "BATTLE");
		e.playerFaction = winner->getFactionName();
		e.territory = input.territoryName;
		ctx.logger->logEvent(e);
	}
}

// Helper: Calculate actual unit strength based on commitment and unit type mix
// Elite units count as 2x strength, normal units as 1x
// When committing X units from (N normal, E elite), commit elite first then normal
int BattlePhase::calculateUnitStrength(int commitUnits, int normalAvailable, int eliteAvailable, int eliteStrength) {
	// Commit elite units first (they're worth more)
	int eliteCommitted = std::min(commitUnits, eliteAvailable);
	int normalCommitted = std::min(commitUnits - eliteCommitted, normalAvailable);
	
	// Strength = normal*1 + elite*eliteStrength
	return normalCommitted + (eliteCommitted * eliteStrength);
}

// Helper: Convert desired strength to unit count
// Elite units count as 2x, normal as 1x. Commits elite first (greedy).
int BattlePhase::convertStrengthToUnitCount(int desiredStrength, int normalAvailable, int eliteAvailable, int eliteStrength) {
	if (eliteStrength <= 1) {
		return std::min(desiredStrength, normalAvailable + eliteAvailable);
	}

	// Commit elite units first to reach strength
	int eliteToCommit = std::min(desiredStrength / eliteStrength, eliteAvailable);
	int strengthFromElite = eliteToCommit * eliteStrength;
	int remainingStrength = desiredStrength - strengthFromElite;
	
	// Fill remaining with normal units
	int normalToCommit = std::min(remainingStrength, normalAvailable);
	
	// Total units committed
	return eliteToCommit + normalToCommit;
}

int BattlePhase::getBattleWheelChoice(PhaseContext& ctx, int playerIndex, int maxUnits,
									  const std::string& territoryName, int eliteStrength) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	// Calculate max strength available
	int maxStrength = 0;
	int normalAvailable = 0, eliteAvailable = 0;
	if (!territoryName.empty()) {
		auto [n, e] = view.map.getCombatUnitBreakdown(territoryName, playerIndex);
		normalAvailable = n;
		eliteAvailable = e;
		maxStrength = calculateUnitStrength(maxUnits, n, e, eliteStrength);
	}
	
	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "int";
		req.actor_index = playerIndex;
		req.prompt = player->getFactionName() + ", enter battle strength (0-" + std::to_string(maxStrength) + "): ";
		req.int_min = 0;
		req.int_max = maxStrength;
		auto resp = ctx.adapter->requestDecision(req);
		int choice = 0;
		if (resp && resp->valid) {
			try { choice = std::stoi(resp->payload_json); } catch (...) {}
		}
		return convertStrengthToUnitCount(choice, normalAvailable, eliteAvailable, eliteStrength);
	} else {
		// AI: random strength choice 0 to maxStrength
		int strength = rand() % (maxStrength + 1);
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " battle strength: " + std::to_string(strength));
		}
		return convertStrengthToUnitCount(strength, normalAvailable, eliteAvailable, eliteStrength);
	}
}

// Ask how many elite units survive based on remaining strength.
// Returns casualties as (normalKilled, eliteKilled).
std::pair<int, int> BattlePhase::askCasualtyDistribution(PhaseContext& ctx, int playerIndex, int remainingStrength,
	                                                     int normalAvailable, int eliteAvailable, int eliteStrength) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	int totalStrength = normalAvailable + (eliteAvailable * eliteStrength);

	// No survivors -> everyone dies.
	if (remainingStrength <= 0) {
		return {normalAvailable, eliteAvailable};
	}

	// All survive.
	if (remainingStrength >= totalStrength) {
		return {0, 0};
	}

	// Feasible elite survivors satisfy:
	// eliteSurvive*eliteStrength + normalSurvive = remainingStrength
	// with 0<=eliteSurvive<=eliteAvailable and 0<=normalSurvive<=normalAvailable.
	int minEliteSurvive = std::max(0, (remainingStrength - normalAvailable + eliteStrength - 1) / eliteStrength);
	int maxEliteSurvive = std::min(eliteAvailable, remainingStrength / eliteStrength);

	// Clamp to a safe feasible value if arithmetic corner-case happens.
	if (minEliteSurvive > maxEliteSurvive) {
		int eliteSurvive = std::max(0, std::min(eliteAvailable, remainingStrength / eliteStrength));
		int normalSurvive = std::max(0, std::min(normalAvailable, remainingStrength - (eliteSurvive * eliteStrength)));
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	}

	// Deterministic case, no need to ask.
	if (minEliteSurvive == maxEliteSurvive) {
		int eliteSurvive = minEliteSurvive;
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	}
	
	if (ctx.adapter) {
		DecisionRequest req;
		req.kind = "int";
		req.actor_index = playerIndex;
		req.prompt = player->getFactionName() + " keeps " + std::to_string(remainingStrength)
			+ " strength. How many elite units survive? ("
			+ std::to_string(minEliteSurvive) + "-" + std::to_string(maxEliteSurvive) + "): ";
		req.int_min = minEliteSurvive;
		req.int_max = maxEliteSurvive;
		auto resp = ctx.adapter->requestDecision(req);
		int eliteSurvive = maxEliteSurvive;
		if (resp && resp->valid) {
			try { eliteSurvive = std::stoi(resp->payload_json); } catch (...) {}
			eliteSurvive = std::max(minEliteSurvive, std::min(maxEliteSurvive, eliteSurvive));
		}
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	} else {
		// AI: keep as many elite as possible among feasible survivor choices.
		int eliteSurvive = maxEliteSurvive;
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		return {normalAvailable - normalSurvive, eliteAvailable - eliteSurvive};
	}
}

