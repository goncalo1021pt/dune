#include "phases/phase_context.hpp"
#include "player.hpp"

FactionAbility* PhaseContext::getAbility(int playerIndex) const {
	if (playerIndex >= 0 && playerIndex < (int)players.size() && players[playerIndex])
		return players[playerIndex]->getFactionAbility();
	return nullptr;
}
