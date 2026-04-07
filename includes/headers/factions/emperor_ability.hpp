#pragma once
#include "factions/faction_ability.hpp"

class EmperorAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
