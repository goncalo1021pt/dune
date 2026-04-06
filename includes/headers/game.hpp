#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <random>

#include "settings.hpp"
#include "map.hpp"
#include "player.hpp"
#include "cards/treachery_deck.hpp"
#include "cards/spice_deck.hpp"
#include "phases/phase.hpp"
#include "phases/ship_and_move_phase.hpp"

enum class gamePhase : int {
	STORM,
    SPICE_BLOW,
    CHOAM_CHARITY,
    BIDDING,
    REVIVAL,
    SHIP_AND_MOVE,
    BATTLE,
    SPICE_COLLECTION,
    MENTAT_PAUSE,
    COUNT
};

static constexpr int NUM_PHASES = static_cast<int>(gamePhase::COUNT);

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
		std::vector<int> turnOrder;  // Player indices in turn order (recalculated after each storm)
		int currentPlayerIndex;

		std::vector<Player*> players;
		int playerCount;

		int stormSector;
		int lastStormCard;
		int nextStormCard;
		bool hasNextStormCard;
		std::vector<int> stormDeck;

		std::vector<int> playerTokenSectors;
		GameMap _map;
		std::mt19937 rng;

		SpiceDeck spiceDeck;

		bool beneGesseritCharity;

		TreacheryDeck treacheryDeck;

		// Phase handlers (Strategy pattern)
		std::vector<std::unique_ptr<Phase>> phases;

		bool interactiveMode;

		bool checkVictory();
		void initializePhases();
		void initializeStormDeck();
		void resolveWormOnTerritory(const std::string& territoryName);
		void setTurnOrder();  // Recalculate turn order based on sector positions

	public:
		Game(int numPlayers, unsigned int seed = 42);
		Game(int numPlayers, unsigned int seed, bool interactive);
		~Game();

		void initializeGame();
		void processPhase();
		void processTurn();
		void runGame();
		
		// Getters
		int getPlayerCount() const;
		int getTurnNumber() const;
		int getStormSector() const;
		int getLastStormCard() const;
		int getNextStormCard() const;
		const Player* getPlayer(int index) const;
		const std::vector<territory>& getTerritories() const;
		bool isInteractiveMode() const;
		const std::vector<int>& getTurnOrder() const;
};