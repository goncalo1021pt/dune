#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <random>

#include "map.hpp"
#include "player.hpp"

#define MAX_PLAYERS 6
#define MAX_TURNS 14

enum class gamePhase {
	STORM,
    SPICE_BLOW,
    CHOAM_CHARITY,
    BIDDING,
    REVIVAL,
    SHIP_AND_MOVE,
    BATTLE,
    SPICE_COLLECTION,
    MENTAT_PAUSE
};

enum class faction {
	ATREIDES,
	HARKONNEN,
	BENE_GESSERIT,
	FREMEN,
	EMPEROR,
	SPACING_GUILD
};

class Game {
	private:
		int turnNumber;
		gamePhase currentPhase;
		std::vector<std::string> turnOrder;
		int currentPlayerIndex;

		std::vector<Player*> players;
		int playerCount;

		int stormSector;
		std::vector<int> playerTokenSectors;  // Token sector for each player (2, 5, 8, 11, 14, 17)
		GameMap _map;
		
		std::mt19937 rng;  // Random number generator (seeded)
		
		// Phase processing
		bool checkVictory();
		void moveStorm();  // Move storm 1-6 sectors counter clockwise
		void phaseSTORM();
		void phaseSPICE_BLOW();
		void phaseCHOAM_CHARITY();
		void phaseBIDDING();
		void phaseREVIVAL();
		void phaseSHIP_AND_MOVE();
		void phaseBATTLE();
		void phaseSPICE_COLLECTION();
		void phaseMENTAT_PAUSE();

	public:
		Game(int numPlayers, unsigned int seed = 42);
		~Game();

		void initializeGame();
		void processPhase();
		void processTurn();
		void runGame();
		
		int getPlayerCount() const;
		int getTurnNumber() const;
		const Player* getPlayer(int index) const;
};