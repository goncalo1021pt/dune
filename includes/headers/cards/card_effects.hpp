#pragma once

#include <string>

// Forward declarations
class Player;
class Leader;

enum class cardEffectType {
	WEAPON_PROJECTILE,
	WEAPON_POISON,
	WEAPON_SPECIAL,
	DEFENSE_PROJECTILE,
	DEFENSE_POISON,
	SPECIAL_LEADER,
	SPECIAL_STORM,
	SPECIAL_MOVE,
	SPECIAL,
	WORTHLESS,
	NONE
};

class CardEffect {
	protected:
		std::string cardName;
		cardEffectType effectType;
		int power;  // Power rating of this effect (varies by card)

	public:
		CardEffect(const std::string& name, cardEffectType type, int pwr);
		virtual ~CardEffect() = default;

		const std::string& getCardName() const;
		cardEffectType getEffectType() const;
		int getPower() const;

		// Apply effect in battle context
		// Returns damage dealt or defense provided
		virtual int applyEffect(Player* attacker, Leader* attackerLeader, 
			                     Player* defender, Leader* defenderLeader) = 0;

		// Clone for copying
		virtual CardEffect* clone() const = 0;
};

class WeaponEffect : public CardEffect {
	public:
		WeaponEffect(const std::string& name, cardEffectType type, int power);
		~WeaponEffect() override = default;

		int applyEffect(Player* attacker, Leader* attackerLeader,
		                Player* defender, Leader* defenderLeader) override;
		CardEffect* clone() const override;
};

class DefenseEffect : public CardEffect {
	public:
		DefenseEffect(const std::string& name, cardEffectType type, int power);
		~DefenseEffect() override = default;

		int applyEffect(Player* attacker, Leader* attackerLeader,
		                Player* defender, Leader* defenderLeader) override;
		CardEffect* clone() const override;
};

class SpecialEffect : public CardEffect {
	public:
		SpecialEffect(const std::string& name, cardEffectType type, int power);
		~SpecialEffect() override = default;

		int applyEffect(Player* attacker, Leader* attackerLeader,
		                Player* defender, Leader* defenderLeader) override;
		CardEffect* clone() const override;
};
