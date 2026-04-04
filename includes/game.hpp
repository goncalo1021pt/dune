#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <random>

#include "map.hpp"
#include "player.hpp"

#define MIN_PLAYERS 2
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
		int lastStormCard;
		int nextStormCard;
		bool hasNextStormCard;
		std::vector<int> stormDeck;
		size_t stormDeckIndex;
		bool beneGesseritCharity;
		std::vector<int> playerTokenSectors;  // Token sector for each player (2, 5, 8, 11, 14, 17)
		GameMap _map;
		
		std::mt19937 rng;  // Random number generator (seeded)
		
		// Phase processing
		bool checkVictory();
		void initializeStormDeck();
		int drawStormCard();
		void moveStorm(int sectorsToMove);  // Move storm counter-clockwise with wraparound
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
		int getStormSector() const;
		int getLastStormCard() const;
		int getNextStormCard() const;
		const Player* getPlayer(int index) const;
};