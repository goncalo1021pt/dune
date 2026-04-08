#pragma once

#include <vector>
#include <memory>
#include <random>

// Forward declarations
class Player;
class GameMap;
class TreacheryDeck;
class TraitorDeck;
class SpiceDeck;
class EventLogger;
class FactionAbility;
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
	TraitorDeck& traitorDeck;

	// Turn order
	std::vector<int>& turnOrder;

	// Rule toggles
	bool& beneGesseritCharity;

	// Random number generator
	std::mt19937& rng;

	// Testing/Debug
	bool interactiveMode;
	
	// Event logging
	EventLogger* logger;

	// Game state
	bool gameEnded;

	struct StormView {
		int turnNumber;
		int& stormSector;
		int& lastStormCard;
		int& nextStormCard;
		bool& hasNextStormCard;
		std::vector<int>& stormDeck;
		GameMap& map;
		std::vector<Player*>& players;
		std::vector<int>& turnOrder;
		std::mt19937& rng;
	};

	/**
	 * SpiceBlowView: SPICE_BLOW Phase (draws and places spice on territories)
	 */
	struct SpiceBlowView {
		SpiceDeck& spiceDeck;
		GameMap& map;
		int turnNumber;
	};

	/**
	 * ChoamCharityView: CHOAM_CHARITY Phase (optional spice distribution)
	 */
	struct ChoamCharityView {
		std::vector<Player*>& players;
		bool& beneGesseritCharity;
	};

	/**
	 * BiddingView: BIDDING Phase (bid for treachery cards, use Bene Gesserit)
	 */
	struct BiddingView {
		std::vector<Player*>& players;
		TreacheryDeck& treacheryDeck;
		bool& beneGesseritCharity;
		std::mt19937& rng;
		bool interactiveMode;
	};

	/**
	 * RevivalView: REVIVAL Phase (revive dead leaders)
	 */
	struct RevivalView {
		std::vector<Player*>& players;
		const std::vector<int>& turnOrder;
		bool interactiveMode;
	};

	/**
	 * ShipAndMoveView: SHIP_AND_MOVE Phase (deploy and move units)
	 */
	struct ShipAndMoveView {
		std::vector<Player*>& players;
		GameMap& map;
		SpiceDeck& spiceDeck;
		const std::vector<int>& turnOrder;
		int turnNumber;
		bool interactiveMode;
	};

	/**
	 * BattleView: BATTLE Phase (resolve battles, kill units/leaders)
	 */
	struct BattleView {
		std::vector<Player*>& players;
		GameMap& map;
		const TreacheryDeck& treacheryDeck;
		const std::vector<int>& turnOrder;
		int turnNumber;
		bool interactiveMode;
	};

	/**
	 * SpiceCollectionView: SPICE_COLLECTION Phase (collect spice from territories and cities)
	 */
	struct SpiceCollectionView {
		GameMap& map;
		std::vector<Player*>& players;
	};

	/**
	 * MentatPauseView: MENTAT_PAUSE Phase (end-of-turn review, victory check)
	 */
	struct MentatPauseView {
		GameMap& map;
		std::vector<Player*>& players;
		int turnNumber;
	};

	// Getter methods for phase views
	StormView getStormView() {
		return StormView {
			turnNumber, stormSector, lastStormCard, nextStormCard, hasNextStormCard,
			stormDeck, map, players, turnOrder, rng
		};
	}

	SpiceBlowView getSpiceBlowView() {
		return SpiceBlowView { spiceDeck, map, turnNumber };
	}

	ChoamCharityView getChoamCharityView() {
		return ChoamCharityView { players, beneGesseritCharity };
	}

	BiddingView getBiddingView() {
		return BiddingView { players, treacheryDeck, beneGesseritCharity, rng, interactiveMode };
	}

	RevivalView getRevivalView() {
		return RevivalView { players, turnOrder, interactiveMode };
	}

	ShipAndMoveView getShipAndMoveView() {
		return ShipAndMoveView { players, map, spiceDeck, turnOrder, turnNumber, interactiveMode };
	}

	BattleView getBattleView() {
		return BattleView { players, map, treacheryDeck, turnOrder, turnNumber, interactiveMode };
	}

	SpiceCollectionView getSpiceCollectionView() {
		return SpiceCollectionView { map, players };
	}

	MentatPauseView getMentatPauseView() {
		return MentatPauseView { map, players, turnNumber };
	}

	// Convenience accessor for faction abilities
	FactionAbility* getAbility(int playerIndex) const;

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
		TraitorDeck& traitorDeck_,
		std::vector<int>& turnOrder_,
		bool& beneGesseritCharity_,
		std::mt19937& rng_,
		bool interactiveMode_ = false,
		EventLogger* logger_ = nullptr
	)
		: turnNumber(turnNumber_), currentPhase(currentPhase_),
		  players(players_), playerCount(playerCount_), map(map_),
		  stormSector(stormSector_), lastStormCard(lastStormCard_),
		  nextStormCard(nextStormCard_), hasNextStormCard(hasNextStormCard_),
		  stormDeck(stormDeck_),
		  spiceDeck(spiceDeck_),
		  treacheryDeck(treacheryDeck_),
		  traitorDeck(traitorDeck_),
		  turnOrder(turnOrder_),
		  beneGesseritCharity(beneGesseritCharity_), rng(rng_),
		  interactiveMode(interactiveMode_), logger(logger_), gameEnded(false) {}
};
