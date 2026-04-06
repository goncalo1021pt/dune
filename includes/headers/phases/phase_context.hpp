#pragma once

#include <vector>
#include <memory>
#include <random>

// Forward declarations
class Player;
class GameMap;
class TreacheryDeck;
class SpiceDeck;
enum class gamePhase : int;

/**
 * PhaseContext: Dependency injection container for all game state
 * that phases need to execute. Reduces coupling and clarifies phase contracts.
 */
struct PhaseContext {
	// Turn and phase state
	int& turnNumber;
	gamePhase& currentPhase;

	// Players and map
	std::vector<Player*>& players;
	int playerCount;
	GameMap& map;

	// Storm state
	int& stormSector;
	int& lastStormCard;
	int& nextStormCard;
	bool& hasNextStormCard;
	std::vector<int>& stormDeck;

	// Spice state
	SpiceDeck& spiceDeck;

	// Treachery state
	TreacheryDeck& treacheryDeck;

	// Turn order
	std::vector<int>& turnOrder;

	// Rule toggles
	bool& beneGesseritCharity;

	// Random number generator
	std::mt19937& rng;

	// Testing/Debug
	bool interactiveMode;

	// Constructor: bind all references
	PhaseContext(
		int& turnNumber_,
		gamePhase& currentPhase_,
		std::vector<Player*>& players_,
		int playerCount_,
		GameMap& map_,
		int& stormSector_,
		int& lastStormCard_,
		int& nextStormCard_,
		bool& hasNextStormCard_,
		std::vector<int>& stormDeck_,
		SpiceDeck& spiceDeck_,
		TreacheryDeck& treacheryDeck_,
		std::vector<int>& turnOrder_,
		bool& beneGesseritCharity_,
		std::mt19937& rng_,
		bool interactiveMode_ = false
	)
		: turnNumber(turnNumber_), currentPhase(currentPhase_),
		  players(players_), playerCount(playerCount_), map(map_),
		  stormSector(stormSector_), lastStormCard(lastStormCard_),
		  nextStormCard(nextStormCard_), hasNextStormCard(hasNextStormCard_),
		  stormDeck(stormDeck_),
		  spiceDeck(spiceDeck_),
		  treacheryDeck(treacheryDeck_),
		  turnOrder(turnOrder_),
		  beneGesseritCharity(beneGesseritCharity_), rng(rng_),
		  interactiveMode(interactiveMode_) {}
};
