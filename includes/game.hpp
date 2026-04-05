#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <random>

#include "settings.hpp"
#include "map.hpp"
#include "player.hpp"
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

enum class spiceCardType {
	LOCATION,
	WORM
};

struct spiceCard {
	spiceCardType type;
	std::string territoryName;
	int spiceAmount;
};

class Game {
	private:
		// Turn and phase management
		int turnNumber;
		gamePhase currentPhase;
		std::vector<std::string> turnOrder;
		int currentPlayerIndex;

		// Players and map
		std::vector<Player*> players;
		int playerCount;

		// Storm state
		int stormSector;
		int lastStormCard;
		int nextStormCard;
		bool hasNextStormCard;
		std::vector<int> stormDeck;

		// Spice state
		bool useExtendedSpiceBlow;
		std::vector<spiceCard> spiceDeck;
		size_t spiceDeckIndex;
		std::vector<spiceCard> spiceDiscardPileA;
		std::vector<spiceCard> spiceDiscardPileB;

		// Rule toggles
		bool beneGesseritCharity;

		// Map and utilities
		std::vector<int> playerTokenSectors;
		GameMap _map;
		std::mt19937 rng;

		// Phase handlers (Strategy pattern)
		std::vector<std::unique_ptr<Phase>> phases;

		// Testing/Debug
		bool interactiveMode;

		// Helper methods
		bool checkVictory();
		void initializePhases();
		void initializeStormDeck();
		void initializeSpiceDeck();
		spiceCard drawSpiceCard();
		void discardSpiceCard(const spiceCard& card, int discardPileIndex);
		void resolveWormOnTerritory(const std::string& territoryName);

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
		bool isInteractiveMode() const { return interactiveMode; }
};