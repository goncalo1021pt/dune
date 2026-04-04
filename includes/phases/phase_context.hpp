#pragma once

#include <vector>
#include <memory>
#include <random>

// Forward declarations
class Player;
class GameMap;
struct spiceCard;
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
	bool& useExtendedSpiceBlow;
	std::vector<spiceCard>& spiceDeck;
	size_t& spiceDeckIndex;
	std::vector<spiceCard>& spiceDiscardPileA;
	std::vector<spiceCard>& spiceDiscardPileB;

	// Rule toggles
	bool& beneGesseritCharity;

	// Random number generator
	std::mt19937& rng;

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
		bool& useExtendedSpiceBlow_,
		std::vector<spiceCard>& spiceDeck_,
		size_t& spiceDeckIndex_,
		std::vector<spiceCard>& spiceDiscardPileA_,
		std::vector<spiceCard>& spiceDiscardPileB_,
		bool& beneGesseritCharity_,
		std::mt19937& rng_
	)
		: turnNumber(turnNumber_), currentPhase(currentPhase_),
		  players(players_), playerCount(playerCount_), map(map_),
		  stormSector(stormSector_), lastStormCard(lastStormCard_),
		  nextStormCard(nextStormCard_), hasNextStormCard(hasNextStormCard_),
		  stormDeck(stormDeck_),
		  useExtendedSpiceBlow(useExtendedSpiceBlow_),
		  spiceDeck(spiceDeck_), spiceDeckIndex(spiceDeckIndex_),
		  spiceDiscardPileA(spiceDiscardPileA_), spiceDiscardPileB(spiceDiscardPileB_),
		  beneGesseritCharity(beneGesseritCharity_), rng(rng_) {}
};
