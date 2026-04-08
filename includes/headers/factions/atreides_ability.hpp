#pragma once
#include "factions/faction_ability.hpp"

class AtreidesAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
