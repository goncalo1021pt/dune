#pragma once
#include "factions/faction_ability.hpp"

class BeneGesseritAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
