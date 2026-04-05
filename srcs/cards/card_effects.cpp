#include "cards/card_effects.hpp"
#include "player.hpp"
#include "leader.hpp"
#include <iostream>

// ==================== CardEffect Base ====================
CardEffect::CardEffect(const std::string& name, cardEffectType type, int pwr)
	: cardName(name), effectType(type), power(pwr) {
}

const std::string& CardEffect::getCardName() const {
	return cardName;
}

cardEffectType CardEffect::getEffectType() const {
	return effectType;
}

int CardEffect::getPower() const {
	return power;
}

// ==================== WeaponEffect ====================
WeaponEffect::WeaponEffect(const std::string& name, cardEffectType type, int pwr)
	: CardEffect(name, type, pwr) {
}

int WeaponEffect::applyEffect(Player* attacker, Leader* attackerLeader,
                              Player* defender, Leader* defenderLeader) {
	if (!attacker || !defender || !attackerLeader || !defenderLeader) {
		return 0;
	}

	// Weapon damage = card power + attacker leader battle value
	int damage = power + attackerLeader->power;
	
	std::cout << "  [WEAPON] " << cardName << " used by " << attacker->getFactionName()
	          << " (leader: " << attackerLeader->name << ")" << std::endl;
	std::cout << "    Damage: " << damage << " (card: " << power 
	          << " + leader: " << attackerLeader->power << ")" << std::endl;
	
	return damage;
}

CardEffect* WeaponEffect::clone() const {
	return new WeaponEffect(cardName, effectType, power);
}

// ==================== DefenseEffect ====================
DefenseEffect::DefenseEffect(const std::string& name, cardEffectType type, int pwr)
	: CardEffect(name, type, pwr) {
}

int DefenseEffect::applyEffect(Player* attacker, Leader* attackerLeader,
                               Player* defender, Leader* defenderLeader) {
	if (!attacker || !defender || !attackerLeader || !defenderLeader) {
		return 0;
	}

	// Defense value = card power + defender leader battle value
	int defense = power + defenderLeader->power;
	
	std::cout << "  [DEFENSE] " << cardName << " used by " << defender->getFactionName()
	          << " (leader: " << defenderLeader->name << ")" << std::endl;
	std::cout << "    Defense: " << defense << " (card: " << power 
	          << " + leader: " << defenderLeader->power << ")" << std::endl;
	
	return defense;
}

CardEffect* DefenseEffect::clone() const {
	return new DefenseEffect(cardName, effectType, power);
}

// ==================== SpecialEffect ====================
SpecialEffect::SpecialEffect(const std::string& name, cardEffectType type, int pwr)
	: CardEffect(name, type, pwr) {
}

int SpecialEffect::applyEffect(Player* attacker, Leader* attackerLeader,
                               Player* defender, Leader* defenderLeader) {
	if (!attacker || !defender || !attackerLeader || !defenderLeader) {
		return 0;
	}

	// Special effects - for now, similar to weapons but with special handling
	int effect = power + attackerLeader->power;
	
	std::cout << "  [SPECIAL] " << cardName << " activated" << std::endl;
	std::cout << "    Effect value: " << effect << std::endl;
	
	return effect;
}

CardEffect* SpecialEffect::clone() const {
	return new SpecialEffect(cardName, effectType, power);
}
