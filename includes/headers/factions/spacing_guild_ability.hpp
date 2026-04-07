#pragma once
#include "factions/faction_ability.hpp"

class SpacingGuildAbility : public FactionAbility {
public:
	std::string getFactionName() const override;
};
