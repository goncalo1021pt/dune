#include "factions/harkonnen_ability.hpp"
#include "player.hpp"
#include "phases/phase_context.hpp"
#include "cards/treachery_deck.hpp"
#include "map.hpp"

std::string HarkonnenAbility::getFactionName() const {
	return "Harkonnen";
}

int HarkonnenAbility::getMaxTreacheryCards() const {
	return 8;  // Harkonnen can hold 8 cards (vs 4 for others)
}

int HarkonnenAbility::getStartingTreacheryCardCount() const {
	return 2;  // Harkonnen start with 2 cards (vs 1 for others)
}

bool HarkonnenAbility::keepsAllTraitorCards() const {
	return true;  // Harkonnen keep ALL traitor cards they draw
}

void HarkonnenAbility::onCardWonAtAuction(PhaseContext& ctx) {
	// Find Harkonnen player
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return;
	
	Player* harkonnen = ctx.players[harkonnenIndex];
	
	// Draw a bonus card if under limit
	if ((int)harkonnen->getTreacheryCards().size() < getMaxTreacheryCards()) {
		treacheryCard bonusCard = ctx.treacheryDeck.drawCard();
		harkonnen->addTreacheryCard(bonusCard.name);
	}
}

void HarkonnenAbility::setupAtStart(Player* player) {
	if (player == nullptr) return;
	player->setSpice(10);  // Harkonnen start with 10 spice
}

void HarkonnenAbility::placeStartingForces(PhaseContext& ctx) {
	// Find Harkonnen player index
	int harkonnenIndex = -1;
	for (size_t i = 0; i < ctx.players.size(); ++i) {
		if (ctx.players[i]->getFactionAbility()->getFactionName() == "Harkonnen") {
			harkonnenIndex = i;
			break;
		}
	}
	
	if (harkonnenIndex < 0) return;
	
	Player* harkonnen = ctx.players[harkonnenIndex];
	
	// Place 10 units in Carthag (sector -1 auto-selects first sector)
	ctx.map.addUnitsToTerritory("Carthag", harkonnenIndex, 10, 0, -1);
	harkonnen->deployUnits(10);
	
	// Remaining 10 units in reserve
}
