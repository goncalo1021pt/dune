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
	  beneGesseritCharity(false),
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
	
	for (int i = 0; i < playerCount; ++i) {
		turnOrder.push_back(FACTION_NAMES[i]);
	}
	
	stormSector = 0;
	initializeStormDeck();
	std::cout << "\nStorm will be placed randomly during Turn 1 STORM phase" << std::endl;
	
	// Initialize player token sectors (2, 5, 8, 11, 14, 17)
	// Pick playerCount tokens evenly spaced from the array
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
	// TODO: Trigger spice blow events
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
	// Revive all destroyed units
	for (int i = 0; i < playerCount; ++i) {
		int destroyed = players[i]->getUnitsDestroyed();
		if (destroyed > 0) {
			players[i]->reviveUnits(destroyed);
			std::cout << "    " << players[i]->getFactionName() << " revives " 
			          << destroyed << " units" << std::endl;
		}
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
