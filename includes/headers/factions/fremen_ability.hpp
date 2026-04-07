#pragma once
#include "factions/faction_ability.hpp"

class FremenAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
