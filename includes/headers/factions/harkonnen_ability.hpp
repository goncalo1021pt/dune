#pragma once
#include "factions/faction_ability.hpp"

class HarkonnenAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
