#include <cards/treachery_deck.hpp>
#include <algorithm>
#include <iostream>

TreacheryDeck::TreacheryDeck(std::mt19937& rng_)
	: deck(), deckIndex(0), rng(rng_) {
}

void TreacheryDeck::initialize() {
	deck.clear();
	deckIndex = 0;

	// All 33 Dune treachery cards with their types and descriptions
	const treacheryCard baseCards[] = {
		{1, "Crysknife", treacheryCardType::WEAPON_PROJECTILE, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Shield. You may keep this card if you win this battle."},
		{2, "Maula Pistol", treacheryCardType::WEAPON_PROJECTILE, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Shield. You may keep this card if you win this battle."},
		{3, "Slip Tip", treacheryCardType::WEAPON_PROJECTILE, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Shield. You may keep this card if you win this battle."},
		{4, "Stunner", treacheryCardType::WEAPON_PROJECTILE, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Shield. You may keep this card if you win this battle."},
		{5, "Lasgun", treacheryCardType::WEAPON_SPECIAL, "Play as part of your Battle Plan. Automatically kills opponent's leader regardless of defense card used. You may keep this card if you win this battle. If anyone plays a Shield in this battle, all forces, leaders, and spice in this battle's territory are lost to the Tleilaxu Tanks and returned to the Spice Bank. Both players lose this battle, no spice is paid for leaders, and all cards played are discarded."},
		{6, "Chaumas", treacheryCardType::WEAPON_POISON, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Snooper. You may keep this card if you win this battle."},
		{7, "Chaumurky", treacheryCardType::WEAPON_POISON, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Snooper. You may keep this card if you win this battle."},
		{8, "Ellaca Drug", treacheryCardType::WEAPON_POISON, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Snooper. You may keep this card if you win this battle."},
		{9, "Gom Jabbar", treacheryCardType::WEAPON_POISON, "Play as part of your Battle Plan. Kills opponent's leader before battle is resolved. Opponent may protect leader with a Snooper. You may keep this card if you win this battle."},
		{10, "Shield", treacheryCardType::DEFENSE_PROJECTILE, "Play as part of your Battle Plan. Protects your leader from a projectile weapon in this battle. You may keep this card if you win this battle."},
		{11, "Shield", treacheryCardType::DEFENSE_PROJECTILE, "Play as part of your Battle Plan. Protects your leader from a projectile weapon in this battle. You may keep this card if you win this battle."},
		{12, "Shield", treacheryCardType::DEFENSE_PROJECTILE, "Play as part of your Battle Plan. Protects your leader from a projectile weapon in this battle. You may keep this card if you win this battle."},
		{13, "Shield", treacheryCardType::DEFENSE_PROJECTILE, "Play as part of your Battle Plan. Protects your leader from a projectile weapon in this battle. You may keep this card if you win this battle."},
		{14, "Snooper", treacheryCardType::DEFENSE_POISON, "Play as part of your Battle Plan. Protects your leader from a poison weapon in this battle. You may keep this card if you win this battle."},
		{15, "Snooper", treacheryCardType::DEFENSE_POISON, "Play as part of your Battle Plan. Protects your leader from a poison weapon in this battle. You may keep this card if you win this battle."},
		{16, "Snooper", treacheryCardType::DEFENSE_POISON, "Play as part of your Battle Plan. Protects your leader from a poison weapon in this battle. You may keep this card if you win this battle."},
		{17, "Snooper", treacheryCardType::DEFENSE_POISON, "Play as part of your Battle Plan. Protects your leader from a poison weapon in this battle. You may keep this card if you win this battle."},
		{18, "Baliset", treacheryCardType::WORTHLESS, "Play as part of your Battle Plan, in place of a weapon, defense, or both. This card has no value in play, and you can discard it only by playing it in your Battle Plan."},
		{19, "Jubba Cloak", treacheryCardType::WORTHLESS, "Play as part of your Battle Plan, in place of a weapon, defense, or both. This card has no value in play, and you can discard it only by playing it in your Battle Plan."},
		{20, "Kulon", treacheryCardType::WORTHLESS, "Play as part of your Battle Plan, in place of a weapon, defense, or both. This card has no value in play, and you can discard it only by playing it in your Battle Plan."},
		{21, "La La La", treacheryCardType::WORTHLESS, "Play as part of your Battle Plan, in place of a weapon, defense, or both. This card has no value in play, and you can discard it only by playing it in your Battle Plan."},
		{22, "Trip to Gamont", treacheryCardType::WORTHLESS, "Play as part of your Battle Plan, in place of a weapon, defense, or both. This card has no value in play, and you can discard it only by playing it in your Battle Plan."},
		{23, "Cheap Hero", treacheryCardType::SPECIAL_LEADER, "Play as a leader with zero strength on your Battle Plan and discard after the battle. You may also play a weapon and a defense. The cheap hero may be played in place of a leader or when you have no leaders available."},
		{24, "Cheap Hero", treacheryCardType::SPECIAL_LEADER, "Play as a leader with zero strength on your Battle Plan and discard after the battle. You may also play a weapon and a defense. The cheap hero may be played in place of a leader or when you have no leaders available."},
		{25, "Cheap Hero", treacheryCardType::SPECIAL_LEADER, "Play as a leader with zero strength on your Battle Plan and discard after the battle. You may also play a weapon and a defense. The cheap hero may be played in place of a leader or when you have no leaders available."},
		{26, "Family Atomics", treacheryCardType::SPECIAL_STORM, "After the first game turn, play after the storm movement is calculated, but before the storm is moved, but only if you have one or more forces on the Shield Wall or a territory adjacent to the Shield Wall with no storm between your sector and the Wall. All forces on the Shield Wall are destroyed. Place the Destroyed Shield Wall token on the Shield Wall as a reminder. The Imperial Basin, Arrakeen and Carthag are no longer protected from the Storm for the rest of the game."},
		{27, "Hajr", treacheryCardType::SPECIAL_MOVE, "Play during Movement Phase. The forces you move may be a group you've already moved this phase or another group. The forces you move make a second on-planet force movement subject to normal movement rules."},
		{28, "Karama", treacheryCardType::SPECIAL, "After game setup and factions have completed their At Start actions, use this card to stop one use of a faction advantage (including alliance abilities) when a player attempts to use it. When used on faction advantages during a battle this must be played before Battle Plans are revealed. Or, this card may be used to do either of these things when appropriate: Purchase a shipment of forces onto the planet at Guild rates. (1 normal) paid to the Spice Bank, or Bid more spice than you have (without revealing this card) and purchase a Treachery Card without paying spice for it (cannot be used if your hand is full)."},
		{29, "Karama", treacheryCardType::SPECIAL, "After game setup and factions have completed their At Start actions, use this card to stop one use of a faction advantage (including alliance abilities) when a player attempts to use it. When used on faction advantages during a battle this must be played before Battle Plans are revealed. Or, this card may be used to do either of these things when appropriate: Purchase a shipment of forces onto the planet at Guild rates. (1 normal) paid to the Spice Bank, or Bid more spice than you have (without revealing this card) and purchase a Treachery Card without paying spice for it (cannot be used if your hand is full)."},
		{30, "Tleilaxu Ghola", treacheryCardType::SPECIAL, "Play at any time to gain an extra revival. You may immediately revive 1 of your leaders regardless of how many leaders you have in the tanks or up to 5 of your forces from the Tleilaxu Tanks to your reserves at no cost in spice. You still get your normal revivals."},
		{31, "Truthtrance", treacheryCardType::SPECIAL, "Publicly ask one other player a single yes/no question about the game that must be answered publicly. The game pauses until an answer is given. The player must answer yes or no truthfully."},
		{32, "Truthtrance", treacheryCardType::SPECIAL, "Publicly ask one other player a single yes/no question about the game that must be answered publicly. The game pauses until an answer is given. The player must answer yes or no truthfully."},
		{33, "Weather Control", treacheryCardType::SPECIAL_STORM, "After the first game turn, play during the Storm Phase before the Storm Marker is moved. When you play this card, you control the storm this phase and may move it from 0 to 10 sectors in a counter-clockwise direction."},
	};

	for (const auto& card : baseCards) {
		deck.push_back(card);
	}

	// Shuffle the deck
	std::shuffle(deck.begin(), deck.end(), rng);

	std::cout << "    Treachery deck initialized with " << deck.size() << " cards" << std::endl;
}

void TreacheryDeck::reshuffle() {
	std::shuffle(deck.begin(), deck.end(), rng);
	deckIndex = 0;
	std::cout << "    Treachery deck reshuffled" << std::endl;
}

treacheryCard TreacheryDeck::drawCard() {
	if (deckIndex >= deck.size()) {
		reshuffle();
	}

	treacheryCard card = deck[deckIndex];
	deckIndex++;
	return card;
}

int TreacheryDeck::remainingCards() const {
	if (deck.empty()) {
		return 0;
	}
	return deck.size() - deckIndex;
}

int TreacheryDeck::getTotalCards() const {
	return deck.size();
}
