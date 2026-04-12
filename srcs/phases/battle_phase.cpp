#include <phases/battle_phase.hpp>
#include <phases/phase_context.hpp>
#include <player.hpp>
#include <leader.hpp>
#include <map.hpp>
#include <cards/treachery_deck.hpp>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cctype>
#include "events/event.hpp"
#include "logger/event_logger.hpp"

namespace {

std::string toLower(std::string value) {
	for (char& ch : value) {
		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}
	return value;
}

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

	auto view = ctx.getBattleView();
	if (!view.interactiveMode) {
		return available[0];
	}

	if (ctx.logger) {
		ctx.logger->logDebug(player->getFactionName() + ", choose a " + label + " card:");
		if (allowNone) {
			ctx.logger->logDebug("  0. None");
		}
		for (size_t i = 0; i < available.size(); ++i) {
			ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + available[i]);
		}
		if (allowNone) {
			ctx.logger->logDebug("Choose (0-" + std::to_string(available.size()) + "): ");
		} else {
			ctx.logger->logDebug("Choose (1-" + std::to_string(available.size()) + "): ");
		}
	}

	while (true) {
		std::string input;
		std::getline(std::cin, input);
		try {
			int choice = std::stoi(input);
			if (allowNone && choice == 0) {
				return "";
			}
			if (choice >= 1 && choice <= static_cast<int>(available.size())) {
				return available[choice - 1];
			}
		} catch (...) {
			for (const std::string& card : available) {
				if (toLower(card) == toLower(input)) {
					return card;
				}
			}
		}

		if (ctx.logger) {
			ctx.logger->logDebug("Invalid choice. Try again: ");
		}
	}
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

	if (ctx.logger) {
		ctx.logger->logDebug(player->getFactionName() + ", select leader for battle:");
	}
	for (int i = 0; i < static_cast<int>(leaders.size()); ++i) {
		if (ctx.logger) {
			ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + leaders[i].name +
				" (power:" + std::to_string(leaders[i].power) + ")");
		}
	}
	int cheapOption = -1;
	if (cheapHeroAvailable) {
		cheapOption = static_cast<int>(leaders.size()) + 1;
		if (ctx.logger) {
			ctx.logger->logDebug("  " + std::to_string(cheapOption) + ". Cheap Hero (power:0)");
		}
	}
	if (ctx.logger) {
		ctx.logger->logDebug("Choose (1-" + std::to_string(static_cast<int>(leaders.size()) + (cheapHeroAvailable ? 1 : 0)) + "): ");
	}

	while (true) {
		std::string input;
		std::getline(std::cin, input);
		try {
			int pick = std::stoi(input);
			if (pick >= 1 && pick <= static_cast<int>(leaders.size())) {
				int idx = pick - 1;
				choice.valid = true;
				choice.aliveLeaderIndex = idx;
				choice.leaderName = leaders[idx].name;
				choice.leaderPower = leaders[idx].power;
				return choice;
			}
			if (cheapHeroAvailable && pick == cheapOption) {
				choice.valid = true;
				choice.usedCheapHero = true;
				choice.leaderName = "Cheap Hero";
				choice.leaderPower = 0;
				return choice;
			}
		} catch (...) {
		}

		if (ctx.logger) {
			ctx.logger->logDebug("Invalid input. Try again: ");
		}
	}
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

	if (!ctx.interactiveMode) {
		voice.active = false;
		return voice;
	}

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Voice] Use Voice against " + targetPlayer->getFactionName() + " this battle? (y/n): ");
	}
	std::string useVoice;
	std::getline(std::cin >> std::ws, useVoice);
	std::string lowered = toLower(useVoice);
	if (!(lowered == "y" || lowered == "yes")) {
		voice.active = false;
		return voice;
	}

	if (ctx.logger) {
		ctx.logger->logDebug("[Bene Voice] Command type:");
		ctx.logger->logDebug("  1. PLAY poison weapon");
		ctx.logger->logDebug("  2. DON'T PLAY poison weapon");
		ctx.logger->logDebug("  3. PLAY projectile weapon");
		ctx.logger->logDebug("  4. DON'T PLAY projectile weapon");
		ctx.logger->logDebug("  5. PLAY lasgun");
		ctx.logger->logDebug("  6. DON'T PLAY lasgun");
		ctx.logger->logDebug("  7. PLAY shield");
		ctx.logger->logDebug("  8. DON'T PLAY shield");
		ctx.logger->logDebug("  9. PLAY snooper");
		ctx.logger->logDebug("  10. DON'T PLAY snooper");
		ctx.logger->logDebug("  11. PLAY worthless card");
		ctx.logger->logDebug("  12. DON'T PLAY worthless card");
		ctx.logger->logDebug("  13. PLAY cheap hero");
		ctx.logger->logDebug("  14. DON'T PLAY cheap hero");
		ctx.logger->logDebug("Choose (1-14): ");
	}

	int cmd = 0;
	while (true) {
		std::string input;
		std::getline(std::cin >> std::ws, input);
		try {
			int pick = std::stoi(input);
			if (pick >= 1 && pick <= 14) {
				cmd = pick;
				break;
			}
		} catch (...) {
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Invalid input. Choose (1-14): ");
		}
	}

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

	if (ctx.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug("[Atreides Prescience] Choose which element to reveal from opponent:");
			ctx.logger->logDebug("  1. Leader");
			ctx.logger->logDebug("  2. Weapon");
			ctx.logger->logDebug("  3. Defense");
			ctx.logger->logDebug("  4. Dial (wheel value)");
			ctx.logger->logDebug("Choose (1-4): ");
		}

		while (true) {
			std::string input;
			std::getline(std::cin >> std::ws, input);
			try {
				int pick = std::stoi(input);
				if (pick >= 1 && pick <= 4) {
					choice = static_cast<BattleElementToPeek>(pick);
					break;
				}
			} catch (...) {
			}
			if (ctx.logger) {
				ctx.logger->logDebug("Invalid input. Choose (1-4): ");
			}
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
				std::vector<std::string> filtered;
				const bool affectsOffense =
					(voice.target == BeneVoiceTarget::WEAPON_POISON ||
					 voice.target == BeneVoiceTarget::WEAPON_PROJECTILE ||
					 voice.target == BeneVoiceTarget::WEAPON_LASGUN ||
					 voice.target == BeneVoiceTarget::WORTHLESS);
				for (const std::string& c : offenseChoices) {
					bool match = cardMatchesVoiceTarget(c, voice.target);
					if ((voice.demandPlay && match) || (!voice.demandPlay && !match)) {
						filtered.push_back(c);
					}
				}
				if (affectsOffense && (!filtered.empty() || !voice.demandPlay)) {
					offenseChoices = filtered;
				}
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
				std::vector<std::string> filtered;
				const bool affectsDefense =
					(voice.target == BeneVoiceTarget::DEFENSE_SHIELD ||
					 voice.target == BeneVoiceTarget::DEFENSE_SNOOPER ||
					 voice.target == BeneVoiceTarget::WORTHLESS);
				for (const std::string& c : defenseChoices) {
					bool match = cardMatchesVoiceTarget(c, voice.target);
					if ((voice.demandPlay && match) || (!voice.demandPlay && !match)) {
						filtered.push_back(c);
					}
				}
				if (affectsDefense && (!filtered.empty() || !voice.demandPlay)) {
					defenseChoices = filtered;
				}
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
			int playerUnits = view.map.getUnitsInTerritory(terr.name, playerIdx);
			if (playerUnits == 0) continue;
			
			// Check if contested (other players also have units)
			bool contested = false;
			for (size_t i = 0; i < view.players.size(); ++i) {
				if (i != (size_t)playerIdx) {
					int otherUnits = view.map.getUnitsInTerritory(terr.name, i);
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
			if (view.interactiveMode && contestedTerritories.size() > 1) {
				// Ask player which battle to resolve first
				if (ctx.logger) {
					ctx.logger->logDebug(attacker->getFactionName() + "'s contested territories:");
				}
				for (size_t i = 0; i < contestedTerritories.size(); ++i) {
					if (ctx.logger) {
						ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + contestedTerritories[i]);
					}
				}
				if (ctx.logger) {
					ctx.logger->logDebug("Choose which to battle (1-" + std::to_string(contestedTerritories.size()) + "): ");
				}
				
				int choice = 0;
				std::string input;
				while (true) {
					std::getline(std::cin, input);
					try {
						choice = std::stoi(input);
						if (choice >= 1 && choice <= (int)contestedTerritories.size()) {
							break;
						}
					} catch (...) {}
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid choice. Try again: ");
					}
				}
				choice--; // Convert to 0-indexed
				
				// Resolve this battle
				resolveBattle(ctx, playerIdx, contestedTerritories[choice]);
				totalBattles++;
				
				// Remove from list
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
			int units = view.map.getUnitsInTerritory(territoryName, i);
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
			int units = view.map.getUnitsInTerritory(territoryName, atreidesPeek.opponentIdx);
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
		int attackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
		int wheelValue = getBattleWheelChoice(ctx, attackerIdx, attackerUnits, territoryName, attackerEliteStrength);
		auto [normalUnits, eliteUnits] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		int unitStrength = calculateUnitStrength(wheelValue, normalUnits, eliteUnits, attackerEliteStrength);

		if (ctx.logger) {
			ctx.logger->logDebug(attacker->getFactionName() + " battle value: " + std::to_string(unitStrength)
				+ " (units: " + std::to_string(wheelValue) + " committed from " + std::to_string(normalUnits)
				+ "N+" + std::to_string(eliteUnits) + "E) + " + std::to_string(attackerLeaderPower)
				+ " (leader) = " + std::to_string(unitStrength + attackerLeaderPower));
		}

		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses dialed strength worth of units, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		
		// Get defender's breakdown for full removal
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
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
				std::vector<std::string> filtered;
				bool affectsOffense = (beneVoice.target == BeneVoiceTarget::WEAPON_POISON ||
					beneVoice.target == BeneVoiceTarget::WEAPON_PROJECTILE ||
					beneVoice.target == BeneVoiceTarget::WEAPON_LASGUN ||
					beneVoice.target == BeneVoiceTarget::WORTHLESS);
				if (affectsOffense) {
					for (const std::string& c : offenseChoices) {
						bool match = cardMatchesVoiceTarget(c, beneVoice.target);
						if ((beneVoice.demandPlay && match) || (!beneVoice.demandPlay && !match)) {
							filtered.push_back(c);
						}
					}
					if (!filtered.empty() || !beneVoice.demandPlay) {
						offenseChoices = filtered;
						if (beneVoice.demandPlay) {
							offenseAllowNone = false;
						}
					}
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
				std::vector<std::string> filtered;
				bool affectsDefense = (beneVoice.target == BeneVoiceTarget::DEFENSE_SHIELD ||
					beneVoice.target == BeneVoiceTarget::DEFENSE_SNOOPER ||
					beneVoice.target == BeneVoiceTarget::WORTHLESS);
				if (affectsDefense) {
					for (const std::string& c : defenseChoices) {
						bool match = cardMatchesVoiceTarget(c, beneVoice.target);
						if ((beneVoice.demandPlay && match) || (!beneVoice.demandPlay && !match)) {
							filtered.push_back(c);
						}
					}
					if (!filtered.empty() || !beneVoice.demandPlay) {
						defenseChoices = filtered;
						if (beneVoice.demandPlay) {
							defenseAllowNone = false;
						}
					}
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
			int units = view.map.getUnitsInTerritory(territoryName, playerIdx);
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
	int attackerUnits = view.map.getUnitsInTerritory(territoryName, attackerIdx);
	int defenderUnits = view.map.getUnitsInTerritory(territoryName, defenderIdx);
	auto [normalUnits, eliteUnits] = view.map.getUnitBreakdown(territoryName, attackerIdx);
	auto [defNormalUnits, defEliteUnits] = view.map.getUnitBreakdown(territoryName, defenderIdx);
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
			if (view.interactiveMode) {
				if (ctx.logger) {
					ctx.logger->logDebug(winner->getFactionName() + ", keep card \"" + cardName + "\"? (y/n): ");
				}
				std::string input;
				std::getline(std::cin, input);
				keep = (input == "y" || input == "Y" || input == "yes" || input == "Yes");
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

	// Determine winner (attacker wins ties)
	if (attackerValue > defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + attacker->getFactionName() + " wins!");
		}
		
		// Winner (attacker) keeps totalStrength - dialedStrength
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses casualties, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				attacker->getFactionName() + " defeats " + defender->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger advanced faction battle-win hooks (e.g., Harkonnen capture)
		if (ctx.featureSettings.advancedFactionAbilities) {
			attacker->getFactionAbility()->onBattleWon(ctx, defenderIdx);
		}
		awardWinnerSpiceForDeadLeaders(attacker);
		resolveBattleCardAftermath(attackerIdx, defenderIdx);
		
	} else if (attackerValue < defenderValue) {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: " + defender->getFactionName() + " wins!");
		}
		
		// Calculate unit breakdowns for removal
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalDefenderStrength = calculateUnitStrength(defenderUnits, defNormalUnits, defEliteUnits, defenderEliteStrength);
		int defenderRemainingStrength = std::max(0, totalDefenderStrength - defUnitStrength);
		auto [defNKilled, defEKilled] = askCasualtyDistribution(ctx, defenderIdx, defenderRemainingStrength, defN, defE, defenderEliteStrength);
		
		// Defender wins: attacker loses all units, defender loses casualties
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aN, aE);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defNKilled, defEKilled);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				defender->getFactionName() + " defeats " + attacker->getFactionName() + " in " + territoryName,
				ctx.turnNumber, "BATTLE");
			e.playerFaction = defender->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger advanced faction battle-win hooks (e.g., Harkonnen capture)
		if (ctx.featureSettings.advancedFactionAbilities) {
			defender->getFactionAbility()->onBattleWon(ctx, attackerIdx);
		}
		awardWinnerSpiceForDeadLeaders(defender);
		resolveBattleCardAftermath(defenderIdx, attackerIdx);
		
	} else {
		if (ctx.logger) {
			ctx.logger->logDebug("Result: Tie! " + attacker->getFactionName() + " wins (attacker advantage).");
		}
		
		// Winner (attacker in tie) keeps totalStrength - dialedStrength
		auto [aN, aE] = view.map.getUnitBreakdown(territoryName, attackerIdx);
		auto [defN, defE] = view.map.getUnitBreakdown(territoryName, defenderIdx);
		
		int totalAttackerStrength = calculateUnitStrength(attackerUnits, normalUnits, eliteUnits, attackerEliteStrength);
		int attackerRemainingStrength = std::max(0, totalAttackerStrength - unitStrength);
		auto [aNKilled, aEKilled] = askCasualtyDistribution(ctx, attackerIdx, attackerRemainingStrength, aN, aE, attackerEliteStrength);
		
		// Attacker loses casualties, defender loses all units
		view.map.removeUnitsFromTerritory(territoryName, attackerIdx, aNKilled, aEKilled);
		view.map.removeUnitsFromTerritory(territoryName, defenderIdx, defN, defE);
		
		if (ctx.logger) {
			Event e(EventType::BATTLE_RESOLVED,
				"Tie in " + territoryName + ": " + attacker->getFactionName() + " wins",
				ctx.turnNumber, "BATTLE");
			e.playerFaction = attacker->getFactionName();
			e.territory = territoryName;
			ctx.logger->logEvent(e);
		}
		
		// Trigger advanced faction battle-win hooks (attacker wins tie)
		if (ctx.featureSettings.advancedFactionAbilities) {
			attacker->getFactionAbility()->onBattleWon(ctx, defenderIdx);
		}
		awardWinnerSpiceForDeadLeaders(attacker);
		resolveBattleCardAftermath(attackerIdx, defenderIdx);
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
		auto [n, e] = view.map.getUnitBreakdown(territoryName, playerIndex);
		normalAvailable = n;
		eliteAvailable = e;
		maxStrength = calculateUnitStrength(maxUnits, n, e, eliteStrength);
	}
	
	if (view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + ", enter battle strength (0-" + std::to_string(maxStrength) + "): ");
		}
		
		int choice = -1;
		while (choice < 0 || choice > maxStrength) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 0 || choice > maxStrength) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Must be 0-" + std::to_string(maxStrength) + ": ");
					}
					choice = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
				choice = -1;
			}
		}
		
		// Convert strength to unit count for removal calculation
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
	
	if (view.interactiveMode) {
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " keeps " + std::to_string(remainingStrength) + " strength after battle.");
			ctx.logger->logDebug("How many elite units survive? (" + std::to_string(minEliteSurvive) + "-" + std::to_string(maxEliteSurvive) + "): ");
		}
		
		int eliteSurvive = -1;
		while (eliteSurvive < minEliteSurvive || eliteSurvive > maxEliteSurvive) {
			std::string input;
			std::getline(std::cin, input);
			try {
				eliteSurvive = std::stoi(input);
				if (eliteSurvive < minEliteSurvive || eliteSurvive > maxEliteSurvive) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Must be " + std::to_string(minEliteSurvive) + "-" + std::to_string(maxEliteSurvive) + ": ");
					}
					eliteSurvive = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
				eliteSurvive = -1;
			}
		}

		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		int normalKilled = normalAvailable - normalSurvive;
		int eliteKilled = eliteAvailable - eliteSurvive;
		return {normalKilled, eliteKilled};
	} else {
		// AI: keep as many elite as possible among feasible survivor choices.
		int eliteSurvive = maxEliteSurvive;
		int normalSurvive = remainingStrength - (eliteSurvive * eliteStrength);
		int normalKilled = normalAvailable - normalSurvive;
		int eliteKilled = eliteAvailable - eliteSurvive;
		return {normalKilled, eliteKilled};
	}
}

int BattlePhase::selectLeaderForBattle(PhaseContext& ctx, int playerIndex, int aliveLeaderCount) {
	auto view = ctx.getBattleView();
	Player* player = view.players[playerIndex];
	
	if (view.interactiveMode && aliveLeaderCount > 1) {
		const auto& leaders = player->getAliveLeaders();
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + ", select leader for battle:");
		}
		for (int i = 0; i < (int)leaders.size(); ++i) {
			if (ctx.logger) {
				ctx.logger->logDebug("  " + std::to_string(i + 1) + ". " + leaders[i].name 
			          + " (power:" + std::to_string(leaders[i].power) + ")");
			}
		}
		if (ctx.logger) {
			ctx.logger->logDebug("Choose (1-" + std::to_string(aliveLeaderCount) + "): ");
		}
		
		int choice = -1;
		while (choice < 1 || choice > aliveLeaderCount) {
			std::string input;
			std::getline(std::cin, input);
			try {
				choice = std::stoi(input);
				if (choice < 1 || choice > aliveLeaderCount) {
					if (ctx.logger) {
						ctx.logger->logDebug("Invalid. Try again: ");
					}
					choice = -1;
				}
			} catch (...) {
				if (ctx.logger) {
					ctx.logger->logDebug("Invalid input. Try again: ");
				}
				choice = -1;
			}
		}
		return choice - 1; // Convert to 0-indexed
	} else {
		// Single leader or AI mode - pick highest power
		const auto& leaders = player->getAliveLeaders();
		int bestIdx = 0;
		int bestPower = -1;
		for (int i = 0; i < (int)leaders.size(); ++i) {
			if (leaders[i].power > bestPower) {
				bestPower = leaders[i].power;
				bestIdx = i;
			}
		}
		if (ctx.logger) {
			ctx.logger->logDebug(player->getFactionName() + " selects: " + leaders[bestIdx].name);
		}
		return bestIdx;
	}
}

