#include "game.hpp"
#include "leader.hpp"
#include "phases/phase_context.hpp"
#include "phases/storm_phase.hpp"
#include "phases/spice_blow_phase.hpp"
#include "phases/ship_and_move_phase.hpp"
#include "phases/spice_collection_phase.hpp"
#include "phases/choam_charity_phase.hpp"
#include "phases/revival_phase.hpp"
#include "phases/bidding_phase.hpp"
#include <iostream>
#include <algorithm>

static const std::string FACTION_NAMES[] = {
	"Atreides",
	"Harkonnen",
	"Fremen",
	"Emperor",
	"Spacing Guild",
	"Bene Gesserit",
};

static std::string getPhaseName(gamePhase phase) {
	switch (phase) {
		case gamePhase::STORM: return "STORM";
		case gamePhase::SPICE_BLOW: return "SPICE_BLOW";
		case gamePhase::CHOAM_CHARITY: return "CHOAM_CHARITY";
		case gamePhase::BIDDING: return "BIDDING";
		case gamePhase::REVIVAL: return "REVIVAL";
		case gamePhase::SHIP_AND_MOVE: return "SHIP_AND_MOVE";
		case gamePhase::BATTLE: return "BATTLE";
		case gamePhase::SPICE_COLLECTION: return "SPICE_COLLECTION";
		case gamePhase::MENTAT_PAUSE: return "MENTAT_PAUSE";
		default: return "UNKNOWN";
	}
}

Game::Game(int numPlayers, unsigned int seed) 
	: turnNumber(0), currentPhase(gamePhase::STORM), turnOrder(), currentPlayerIndex(0),
	  players(), playerCount(numPlayers), stormSector(0), lastStormCard(0),
	  nextStormCard(0), hasNextStormCard(false), stormDeck(),
	  useExtendedSpiceBlow(false), spiceDeck(), spiceDeckIndex(0),
	  spiceDiscardPileA(), spiceDiscardPileB(), beneGesseritCharity(false),
	  playerTokenSectors(), _map(), rng(seed), treacheryDeck(rng), phases(), interactiveMode(false) {
	
	if (playerCount < MIN_PLAYERS || playerCount > MAX_PLAYERS) {
		throw std::invalid_argument("Number of players must be between " + 
			std::to_string(MIN_PLAYERS) + " and " + std::to_string(MAX_PLAYERS));
	}
	
	for (int i = 0; i < playerCount; ++i) {
		players.push_back(new Player(i, FACTION_NAMES[i]));
		// Assign all 5 default leaders (power 1-5) to each faction
		for (int power = 1; power <= 5; ++power) {
			players.back()->addLeader(Leader::createForFaction(FACTION_NAMES[i], power));
		}
	}

	initializePhases();
}

Game::Game(int numPlayers, unsigned int seed, bool interactive)
	: turnNumber(0), currentPhase(gamePhase::STORM), turnOrder(), currentPlayerIndex(0),
	  players(), playerCount(numPlayers), stormSector(0), lastStormCard(0),
	  nextStormCard(0), hasNextStormCard(false), stormDeck(),
	  useExtendedSpiceBlow(false), spiceDeck(), spiceDeckIndex(0),
	  spiceDiscardPileA(), spiceDiscardPileB(), beneGesseritCharity(false),
	  playerTokenSectors(), _map(), rng(seed), treacheryDeck(rng), phases(), interactiveMode(interactive) {
	
	if (playerCount < MIN_PLAYERS || playerCount > MAX_PLAYERS) {
		throw std::invalid_argument("Number of players must be between " + 
			std::to_string(MIN_PLAYERS) + " and " + std::to_string(MAX_PLAYERS));
	}
	
	for (int i = 0; i < playerCount; ++i) {
		players.push_back(new Player(i, FACTION_NAMES[i]));
		// Assign all 5 default leaders (power 1-5) to each faction
		for (int power = 1; power <= 5; ++power) {
			players.back()->addLeader(Leader::createForFaction(FACTION_NAMES[i], power));
		}
	}

	initializePhases();
}

Game::~Game() {
	for (auto p : players) {
		delete p;
	}
	players.clear();
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

bool Game::isInteractiveMode() const { 
	return interactiveMode; 
}

void Game::initializeGame() {
	std::cout << "\n=== Initializing Dune Game ===" << std::endl;
	std::cout << "Number of players: " << playerCount << std::endl;
	
	_map.initializeMap();
	std::cout << "Map initialized with " << _map.getTerritories().size() << " territories" << std::endl;
	initializeSpiceDeck();
	treacheryDeck.initialize();
	
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
		
		// Print leader info
		const auto& aliveLeaders = players[i]->getAliveLeaders();
		if (!aliveLeaders.empty()) {
			std::cout << "    Leaders: ";
			for (size_t j = 0; j < aliveLeaders.size(); j++) {
				std::cout << aliveLeaders[j].name << " (power:" << aliveLeaders[j].power << ")";
				if (j < aliveLeaders.size() - 1) std::cout << ", ";
			}
			std::cout << std::endl;
		}
	}
	
	std::cout << "=== Game Initialized ===" << std::endl;
}

void Game::initializePhases() {
	phases.clear();
	phases.resize(NUM_PHASES);
	phases[static_cast<int>(gamePhase::STORM)]          = std::make_unique<StormPhase>();
	phases[static_cast<int>(gamePhase::SPICE_BLOW)]     = std::make_unique<SpiceBlowPhase>();
	phases[static_cast<int>(gamePhase::CHOAM_CHARITY)]  = std::make_unique<ChoamCharityPhase>();
	phases[static_cast<int>(gamePhase::BIDDING)]        = std::make_unique<BiddingPhase>();
	phases[static_cast<int>(gamePhase::REVIVAL)]        = std::make_unique<RevivalPhase>();
	phases[static_cast<int>(gamePhase::SHIP_AND_MOVE)]  = std::make_unique<ShipAndMovePhase>();
	// phases[static_cast<int>(gamePhase::BATTLE)]      = BATTLE (TODO)
	phases[static_cast<int>(gamePhase::SPICE_COLLECTION)] = std::make_unique<SpiceCollectionPhase>();
	// phases[static_cast<int>(gamePhase::MENTAT_PAUSE)] = MENTAT_PAUSE (TODO)
}

void Game::initializeStormDeck() {
	stormDeck = {1, 2, 3, 4, 5, 6};
	std::shuffle(stormDeck.begin(), stormDeck.end(), rng);
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

const Player* Game::getPlayer(int index) const {
	if (index >= 0 && index < playerCount) {
		return players[index];
	}
	return nullptr;
}

const std::vector<territory>& Game::getTerritories() const {
	return _map.getTerritories();
}

const std::vector<int>& Game::getTurnOrder() const {
	return turnOrder;
}

void Game::setTurnOrder() {
	// Clear and recalculate turn order based on sector positions
	// Counter-clockwise from storm sector (sector numbers increasing)
	// First player is in the sector right after the storm
	
	turnOrder.clear();
	
	// Create list of (player index, next sector counter-clockwise from storm)
	std::vector<std::pair<int, int>> playersWithSectors;
	
	for (int i = 0; i < playerCount; ++i) {
		int tokenSector = playerTokenSectors[i];
		// Distance counter-clockwise from storm to this player's token
		int distance = (tokenSector - stormSector + 18) % 18;
		playersWithSectors.push_back({i, distance});
	}
	
	// Sort by distance (smallest distance = closest counter-clockwise = first in turn order)
	std::sort(playersWithSectors.begin(), playersWithSectors.end(),
		[](const auto& a, const auto& b) { return a.second < b.second; });
	
	// Build turn order from sorted list
	for (const auto& pair : playersWithSectors) {
		turnOrder.push_back(pair.first);
	}
	
	// Print the calculated turn order
	std::cout << "\nTurn Order (Counter-clockwise from Storm at Sector " << stormSector << "):" << std::endl;
	for (size_t i = 0; i < turnOrder.size(); ++i) {
		int playerIdx = turnOrder[i];
		int tokenSector = playerTokenSectors[playerIdx];
		int distance = (tokenSector - stormSector + 18) % 18;
		std::cout << "  " << (i + 1) << ". " << players[playerIdx]->getFactionName() 
				  << " (Sector " << tokenSector << ", distance " << distance << ")" << std::endl;
	}
	std::cout << std::endl;
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

void Game::processPhase() {
	PhaseContext ctx(
		turnNumber, currentPhase, players, playerCount, _map,
		stormSector, lastStormCard, nextStormCard, hasNextStormCard,
		stormDeck, useExtendedSpiceBlow,
		spiceDeck, spiceDeckIndex, spiceDiscardPileA, spiceDiscardPileB,
		treacheryDeck, beneGesseritCharity, rng, interactiveMode
	);

	if (phases[static_cast<int>(currentPhase)]) {
		phases[static_cast<int>(currentPhase)]->execute(ctx);
	} else {
		std::cout << "  " << getPhaseName(currentPhase) << " Phase (TODO)" << std::endl;
	}
	
	// Update turn order after storm phase
	if (currentPhase == gamePhase::STORM) {
		setTurnOrder();
	}
}

void Game::processTurn() {
	turnNumber++;
	std::cout << "\n--- Turn " << turnNumber << " ---" << std::endl;
	
	// Reset leader battle status at start of turn
	for (int i = 0; i < playerCount; ++i) {
		players[i]->resetLeaderBattleStatus();
	}
	
	// Print faction leaders
	std::cout << "Leaders at start of turn: " << std::endl;
	for (int i = 0; i < playerCount; ++i) {
		const auto& aliveLeaders = players[i]->getAliveLeaders();
		if (!aliveLeaders.empty()) {
			std::cout << "  " << players[i]->getFactionName() << ": ";
			for (size_t j = 0; j < aliveLeaders.size(); j++) {
				std::cout << aliveLeaders[j].name << " (power:" << aliveLeaders[j].power << ")";
				if (j < aliveLeaders.size() - 1) std::cout << ", ";
			}
			std::cout << "\n";
		}
	}
	
	// Execute all phases
	for (int i = 0; i < NUM_PHASES; ++i) {
		currentPhase = static_cast<gamePhase>(i);
		processPhase();
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

