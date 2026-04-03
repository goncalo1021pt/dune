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
	  players(), playerCount(numPlayers), stormSector(0), _map(), rng(seed) {
	
	// Validate player count
	if (playerCount < 2 || playerCount > MAX_PLAYERS) {
		playerCount = 2;
	}
	
	// Create players for this game
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
	std::cout << "Map initialized with 5 territories" << std::endl;
	
	for (int i = 0; i < playerCount; ++i) {
		turnOrder.push_back(FACTION_NAMES[i]);
	}
	
	std::cout << "\nPlayers initialized:" << std::endl;
	for (int i = 0; i < playerCount; ++i) {
		std::cout << "  " << players[i]->getFactionName() << ": " 
		          << players[i]->getSpice() << " spice, " 
		          << players[i]->getUnitsReserve() << " units in reserve" << std::endl;
	}
	
	std::cout << "=== Game Initialized ===" << std::endl;
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
	// TODO: Move storm, apply damage
}

void Game::phaseSPICE_BLOW() {
	std::cout << "  SPICE_BLOW Phase" << std::endl;
	// TODO: Trigger spice blow events
}

void Game::phaseCHOAM_CHARITY() {
	std::cout << "  CHOAM_CHARITY Phase" << std::endl;
	// TODO: Distribute spice from CHOAM charity
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
