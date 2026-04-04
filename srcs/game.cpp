#include "game.hpp"
#include <iostream>
#include <algorithm>

static const std::string FACTION_NAMES[] = {
	"Atreides",
	"Harkonnen",
	"Bene Gesserit",
	"Fremen",
	"Emperor",
	"Spacing Guild"
};

Game::Game(int numPlayers, unsigned int seed) 
	: turnNumber(0), currentPhase(gamePhase::STORM), turnOrder(), currentPlayerIndex(0),
	  players(), playerCount(numPlayers), stormSector(0), lastStormCard(0),
	  nextStormCard(0), hasNextStormCard(false), stormDeck(), stormDeckIndex(0),
	  beneGesseritCharity(false), useExtendedSpiceBlow(false),
	  spiceDeck(), spiceDeckIndex(0), spiceDiscardPileA(), spiceDiscardPileB(),
	  _map(), rng(seed) {
	
	if (playerCount < MIN_PLAYERS || playerCount > MAX_PLAYERS) {
		throw std::invalid_argument("Number of players must be between " + 
			std::to_string(MIN_PLAYERS) + " and " + std::to_string(MAX_PLAYERS));
	}
	
	for (int i = 0; i < playerCount; ++i) {
		players.push_back(new Player(i, FACTION_NAMES[i]));
	}
}

Game::~Game() {
	for (auto p : players) {
		delete p;
	}
	players.clear();
}

void Game::initializeGame() {
	std::cout << "\n=== Initializing Dune Game ===" << std::endl;
	std::cout << "Number of players: " << playerCount << std::endl;
	
	_map.initializeMap();
	std::cout << "Map initialized with " << _map.getTerritories().size() << " territories" << std::endl;
	initializeSpiceDeck();
	
	for (int i = 0; i < playerCount; ++i) {
		turnOrder.push_back(FACTION_NAMES[i]);
	}
	
	stormSector = 0;
	initializeStormDeck();
	std::cout << "\nStorm will be placed randomly during Turn 1 STORM phase" << std::endl;
	
	int tokenSectors[] = {2, 5, 8, 11, 14, 17};
	std::uniform_int_distribution<> offsetDist(0, 5);
	int randomOffset = offsetDist(rng);
	
	playerTokenSectors.clear();
	int spacing = 6 / playerCount;
	for (int i = 0; i < playerCount; ++i) {
		int index = (randomOffset + i * spacing) % 6;
		playerTokenSectors.push_back(tokenSectors[index]);
	}
	std::cout << "Player tokens placed at sectors:";
	for (int i = 0; i < playerCount; ++i) {
		std::cout << " P" << (i + 1) << "-" << playerTokenSectors[i];
	}
	std::cout << std::endl;
	
	std::cout << "\nPlayers initialized:" << std::endl;
	for (int i = 0; i < playerCount; ++i) {
		std::cout << "  " << players[i]->getFactionName() << ": " 
		          << players[i]->getSpice() << " spice, " 
		          << players[i]->getUnitsReserve() << " units in reserve" << std::endl;
	}
	
	std::cout << "=== Game Initialized ===" << std::endl;
}

void Game::initializeStormDeck() {
	stormDeck = {1, 2, 3, 4, 5, 6};
	std::shuffle(stormDeck.begin(), stormDeck.end(), rng);
	stormDeckIndex = 0;
}

void Game::initializeSpiceDeck() {
	spiceDeck.clear();
	spiceDiscardPileA.clear();
	spiceDiscardPileB.clear();
	spiceDeckIndex = 0;

	for (const auto& terr : _map.getTerritories()) {
		territory* mutableTerritory = _map.getTerritory(terr.name);
		if (mutableTerritory == nullptr) {
			continue;
		}

		if (terr.terrain == terrainType::desert && terr.spiceAmount > 0) {
			spiceCard locationCard;
			locationCard.type = spiceCardType::LOCATION;
			locationCard.territoryName = terr.name;
			locationCard.spiceAmount = terr.spiceAmount;
			spiceDeck.push_back(locationCard);
		}

		mutableTerritory->spiceAmount = 0;
	}

	for (int i = 0; i < 6; ++i) {
		spiceCard wormCard;
		wormCard.type = spiceCardType::WORM;
		wormCard.territoryName = "";
		wormCard.spiceAmount = 0;
		spiceDeck.push_back(wormCard);
	}

	std::shuffle(spiceDeck.begin(), spiceDeck.end(), rng);
}

spiceCard Game::drawSpiceCard() {
	if (spiceDeckIndex >= spiceDeck.size()) {
		if (spiceDiscardPileA.empty() && spiceDiscardPileB.empty()) {
			spiceCard fallback;
			fallback.type = spiceCardType::WORM;
			fallback.territoryName = "";
			fallback.spiceAmount = 0;
			return fallback;
		}

		spiceDeck.clear();
		spiceDeck.insert(spiceDeck.end(), spiceDiscardPileA.begin(), spiceDiscardPileA.end());
		spiceDeck.insert(spiceDeck.end(), spiceDiscardPileB.begin(), spiceDiscardPileB.end());
		spiceDiscardPileA.clear();
		spiceDiscardPileB.clear();
		std::shuffle(spiceDeck.begin(), spiceDeck.end(), rng);
		spiceDeckIndex = 0;
		std::cout << "    Spice deck reshuffled from discard piles" << std::endl;
	}

	spiceCard card = spiceDeck[spiceDeckIndex];
	spiceDeckIndex++;
	return card;
}

void Game::discardSpiceCard(const spiceCard& card, int discardPileIndex) {
	if (useExtendedSpiceBlow && discardPileIndex == 1) {
		spiceDiscardPileB.push_back(card);
		return;
	}
	spiceDiscardPileA.push_back(card);
}

void Game::resolveWormOnTerritory(const std::string& territoryName) {
	territory* terr = _map.getTerritory(territoryName);
	if (terr == nullptr) {
		return;
	}

	int totalUnitsKilled = 0;
	for (const auto& stack : terr->unitsPresent) {
		totalUnitsKilled += stack.normal_units + stack.elite_units;
	}

	terr->unitsPresent.clear();
	int spiceDestroyed = terr->spiceAmount;
	terr->spiceAmount = 0;

	std::cout << "    Worm devours " << territoryName << ": "
	          << totalUnitsKilled << " units and "
	          << spiceDestroyed << " spice removed" << std::endl;
}

int Game::drawStormCard() {
	if (stormDeckIndex >= stormDeck.size()) {
		initializeStormDeck();
	}

	int card = stormDeck[stormDeckIndex];
	stormDeckIndex++;
	return card;
}

const Player* Game::getPlayer(int index) const {
	if (index >= 0 && index < playerCount) {
		return players[index];
	}
	return nullptr;
}

bool Game::checkVictory() {
	for (int i = 0; i < playerCount; ++i) {
		if (_map.countControlledTerritories(i) >= 3) {
			std::cout << "\n*** VICTORY! " << players[i]->getFactionName() 
			          << " controls 3 territories! ***\n" << std::endl;
			return true;
		}
	}
	return false;
}

void Game::phaseSTORM() {
	std::cout << "  STORM Phase" << std::endl;

	if (turnNumber == 1) {
		std::uniform_int_distribution<> startSectorDist(1, 18);
		stormSector = startSectorDist(rng);
		lastStormCard = 0;
		nextStormCard = drawStormCard();
		hasNextStormCard = true;

		std::cout << "    First turn setup: storm placed at random sector " << stormSector << std::endl;
		std::cout << "    Next storm card prepared: " << nextStormCard << std::endl;
		return;
	}

	if (!hasNextStormCard) {
		nextStormCard = drawStormCard();
		hasNextStormCard = true;
	}

	lastStormCard = nextStormCard;
	hasNextStormCard = false;
	moveStorm(lastStormCard);

	nextStormCard = drawStormCard();
	hasNextStormCard = true;

	std::cout << "    Storm card resolved: " << lastStormCard << std::endl;
	std::cout << "    Storm now at sector " << stormSector << std::endl;
	std::cout << "    Next storm card prepared: " << nextStormCard << std::endl;
}

void Game::moveStorm(int sectorsToMove) {
	stormSector += sectorsToMove;
	while (stormSector > 18) {
		stormSector -= 18;
	}
	while (stormSector < 1) {
		stormSector += 18;
	}
}

void Game::phaseSPICE_BLOW() {
	std::cout << "  SPICE_BLOW Phase" << std::endl;

	const int blowCount = useExtendedSpiceBlow ? 2 : 1;

	for (int blowIndex = 0; blowIndex < blowCount; ++blowIndex) {
		spiceCard card = drawSpiceCard();
		const int discardPileIndex = useExtendedSpiceBlow ? blowIndex : 0;
		const std::vector<spiceCard>& targetDiscardPile =
			(useExtendedSpiceBlow && discardPileIndex == 1) ? spiceDiscardPileB : spiceDiscardPileA;

		if (card.type == spiceCardType::WORM) {
			std::cout << "    Draw " << (blowIndex + 1) << ": WORM" << std::endl;

			if (turnNumber > 1) {
				std::cout << "    NEXUS triggered (alliances not implemented yet)" << std::endl;
			}

			if (targetDiscardPile.empty()) {
				std::cout << "    Worm discarded (no prior discarded card in this pile)" << std::endl;
			} else {
				const spiceCard& topDiscardCard = targetDiscardPile.back();
				if (topDiscardCard.type != spiceCardType::LOCATION) {
					std::cout << "    Worm resolves with no effect (top discard card is WORM)" << std::endl;
				} else {
					territory* targetTerritory = _map.getTerritory(topDiscardCard.territoryName);
					if (targetTerritory == nullptr) {
						std::cout << "    Worm resolves with no effect (invalid target territory)" << std::endl;
					} else if (targetTerritory->spiceAmount <= 0) {
						std::cout << "    Worm resolves on " << topDiscardCard.territoryName
						          << " with no effect (no spice present)" << std::endl;
					} else {
						resolveWormOnTerritory(topDiscardCard.territoryName);
					}
				}
			}

			discardSpiceCard(card, discardPileIndex);
			continue;
		}

		territory* terr = _map.getTerritory(card.territoryName);
		if (terr == nullptr) {
			std::cout << "    Draw " << (blowIndex + 1)
			          << ": invalid territory card discarded" << std::endl;
			discardSpiceCard(card, discardPileIndex);
			continue;
		}

		terr->spiceAmount += card.spiceAmount;

		std::cout << "    Draw " << (blowIndex + 1) << ": "
		          << card.territoryName << " (" << card.spiceAmount << " spice)"
		          << " -> territory now has " << terr->spiceAmount << std::endl;

		discardSpiceCard(card, discardPileIndex);
	}
}

void Game::phaseCHOAM_CHARITY() {
	std::cout << "  CHOAM_CHARITY Phase" << std::endl;
	for (int i = 0; i < playerCount; ++i) {
		int currentSpice = players[i]->getSpice();
		bool isBeneGesserit = (players[i]->getFactionIndex() == static_cast<int>(faction::BENE_GESSERIT));

		bool shouldReceiveCharity = (currentSpice <= 1);
		if (beneGesseritCharity && isBeneGesserit) {
			shouldReceiveCharity = true;
		}

		if (shouldReceiveCharity && currentSpice < 2) {
			int charityAmount = 2 - currentSpice;
			players[i]->addSpice(charityAmount);
			std::cout << "    " << players[i]->getFactionName() << " receives "
			          << charityAmount << " spice from CHOAM (now at "
			          << players[i]->getSpice() << ")" << std::endl;
		}
	}
}

void Game::phaseBIDDING() {
	std::cout << "  BIDDING Phase" << std::endl;
	// TODO: Players bid for leaders
}

void Game::phaseREVIVAL() {
	std::cout << "  REVIVAL Phase" << std::endl;
	const int maxRevivesPerTurn = 3;
	const int spiceCostPerPaidRevive = 2;

	for (int i = 0; i < playerCount; ++i) {
		int destroyed = players[i]->getUnitsDestroyed();
		if (destroyed <= 0) {
			continue;
		}

		int freeRevives = players[i]->getFreeRevivesPerTurn();
		int revivedForFree = std::min(std::min(freeRevives, maxRevivesPerTurn), destroyed);

		int reviveSlotsLeft = maxRevivesPerTurn - revivedForFree;
		int destroyedAfterFree = destroyed - revivedForFree;
		int affordablePaidRevives = players[i]->getSpice() / spiceCostPerPaidRevive;
		int revivedPaid = std::min(std::min(reviveSlotsLeft, destroyedAfterFree), affordablePaidRevives);
		int spicePaid = revivedPaid * spiceCostPerPaidRevive;
		int totalRevived = revivedForFree + revivedPaid;

		if (totalRevived <= 0) {
			continue;
		}

		if (spicePaid > 0) {
			players[i]->removeSpice(spicePaid);
		}
		players[i]->reviveUnits(totalRevived);

		std::cout << "    " << players[i]->getFactionName() << " revives "
		          << totalRevived << " units (" << revivedForFree << " free";
		if (revivedPaid > 0) {
			std::cout << ", " << revivedPaid << " paid for " << spicePaid << " spice";
		}
		std::cout << ")" << std::endl;
	}
}

void Game::phaseSHIP_AND_MOVE() {
	std::cout << "  SHIP_AND_MOVE Phase" << std::endl;
	// TODO: Players move units to territories
}

void Game::phaseBATTLE() {
	std::cout << "  BATTLE Phase" << std::endl;
	// TODO: Resolve battles
}

void Game::phaseSPICE_COLLECTION() {
	std::cout << "  SPICE_COLLECTION Phase" << std::endl;
	// Players collect spice from territories they control
	for (int i = 0; i < playerCount; ++i) {
		int spiceGain = 0;
		for (const auto& terr : _map.getTerritories()) {
			if (_map.isControlled(terr.name, i)) {
				spiceGain += terr.spiceAmount;
			}
		}
		if (spiceGain > 0) {
			players[i]->addSpice(spiceGain);
			std::cout << "    " << players[i]->getFactionName() << " gains " 
			          << spiceGain << " spice" << std::endl;
		}
	}
}

void Game::phaseMENTAT_PAUSE() {
	std::cout << "  MENTAT_PAUSE Phase" << std::endl;
	// Check victory condition
	for (int i = 0; i < playerCount; ++i) {
		int controlled = _map.countControlledTerritories(i);
		std::cout << "    " << players[i]->getFactionName() << " controls " 
		          << controlled << " territories, has " << players[i]->getSpice() 
		          << " spice" << std::endl;
	}
}

void Game::processPhase() {
	switch (currentPhase) {
		case gamePhase::STORM:
			phaseSTORM();
			break;
		case gamePhase::SPICE_BLOW:
			phaseSPICE_BLOW();
			break;
		case gamePhase::CHOAM_CHARITY:
			phaseCHOAM_CHARITY();
			break;
		case gamePhase::BIDDING:
			phaseBIDDING();
			break;
		case gamePhase::REVIVAL:
			phaseREVIVAL();
			break;
		case gamePhase::SHIP_AND_MOVE:
			phaseSHIP_AND_MOVE();
			break;
		case gamePhase::BATTLE:
			phaseBATTLE();
			break;
		case gamePhase::SPICE_COLLECTION:
			phaseSPICE_COLLECTION();
			break;
		case gamePhase::MENTAT_PAUSE:
			phaseMENTAT_PAUSE();
			break;
	}
}

void Game::processTurn() {
	turnNumber++;
	std::cout << "\n--- Turn " << turnNumber << " ---" << std::endl;
	
	// Execute all 9 phases
	currentPhase = gamePhase::STORM;
	for (int phase = 0; phase < 9; ++phase) {
		processPhase();
		currentPhase = static_cast<gamePhase>(static_cast<int>(currentPhase) + 1);
	}
}

void Game::runGame() {
	initializeGame();
	
	while (turnNumber < MAX_TURNS) {
		processTurn();
		
		if (checkVictory()) {
			break;
		}
		
		std::cout << std::endl;
	}
	
	if (turnNumber >= MAX_TURNS) {
		std::cout << "\n=== GAME ENDED: Max turns reached ===" << std::endl;
	}
}

// Getter implementations
int Game::getPlayerCount() const { 
	return playerCount; 
}

int Game::getTurnNumber() const { 
	return turnNumber; 
}

int Game::getStormSector() const {
	return stormSector;
}

int Game::getLastStormCard() const {
	return lastStormCard;
}

int Game::getNextStormCard() const {
	if (!hasNextStormCard) {
		return 0;
	}
	return nextStormCard;
}
