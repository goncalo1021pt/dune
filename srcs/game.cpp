#include "game.hpp"
#include "leader.hpp"
#include "logger/console_event_logger.hpp"
#include "logger/bus_bridge_logger.hpp"
#include "interaction/tty_adapter.hpp"
#include "phases/phase_context.hpp"
#include "phases/storm_phase.hpp"
#include "phases/spice_blow_phase.hpp"
#include "phases/ship_and_move_phase.hpp"
#include "phases/spice_collection_phase.hpp"
#include "phases/choam_charity_phase.hpp"
#include "phases/revival_phase.hpp"
#include "phases/bidding_phase.hpp"
#include "phases/battle_phase.hpp"
#include "phases/mentat_pause_phase.hpp"
#include "factions/atreides_ability.hpp"
#include "factions/harkonnen_ability.hpp"
#include "factions/fremen_ability.hpp"
#include "factions/emperor_ability.hpp"
#include "factions/spacing_guild_ability.hpp"
#include "factions/bene_gesserit_ability.hpp"
#include <iostream>
#include <algorithm>

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

Game::Game(int numPlayers, unsigned int seed, bool interactive, GameFeatureSettings featureSettings)
	: turnNumber(0), currentPhase(gamePhase::STORM), turnOrder(), currentPlayerIndex(0),
	  players(), playerCount(numPlayers), stormSector(0), lastStormCard(0),
	  nextStormCard(0), hasNextStormCard(false), stormDeck(),
	  playerTokenSectors(), _map(), rng(seed), spiceDeck(rng), beneGesseritCharity(false),
	  treacheryDeck(rng), traitorDeck(rng), phases(), interactiveMode(interactive),
	  eventBus(),
	  eventLogger(std::make_unique<BusBridgeLogger>(
		  std::make_unique<ConsoleEventLogger>(), eventBus)),
	  gameEnded(false),
	  featureSettings(featureSettings) {
	if (interactive) {
		interactionAdapter = std::make_unique<TtyAdapter>();
	}
	
	if (playerCount < MIN_PLAYERS || playerCount > MAX_PLAYERS) {
		throw std::invalid_argument("Number of players must be between " + 
			std::to_string(MIN_PLAYERS) + " and " + std::to_string(MAX_PLAYERS));
	}
	
	for (int i = 0; i < playerCount; ++i) {
		players.push_back(new Player(i, FACTION_NAMES[i]));
		
		// Initialize faction-specific leaders from CSV data
		switch (i) {
			case (int)faction::ATREIDES:
				players.back()->addLeader(Leader("Thufir Hawat", 5));
				players.back()->addLeader(Leader("Lady Jessica", 5));
				players.back()->addLeader(Leader("Gurney Halleck", 4));
				players.back()->addLeader(Leader("Duncan Idaho", 2));
				players.back()->addLeader(Leader("Dr. Wellington Yueh", 1));
				break;
			case (int)faction::HARKONNEN:
				players.back()->addLeader(Leader("Feyd-Rautha", 6));
				players.back()->addLeader(Leader("Beast Rabban", 4));
				players.back()->addLeader(Leader("Piter de Vries", 3));
				players.back()->addLeader(Leader("Captain Iakin Nefud", 2));
				players.back()->addLeader(Leader("Umman Kudu", 1));
				break;
			case (int)faction::FREMEN:
				players.back()->addLeader(Leader("Stilgar", 7));
				players.back()->addLeader(Leader("Chani", 6));
				players.back()->addLeader(Leader("Otheym", 5));
				players.back()->addLeader(Leader("Shadout Mapes", 3));
				players.back()->addLeader(Leader("Jamis", 2));
				break;
			case (int)faction::EMPEROR:
				players.back()->addLeader(Leader("Hasimir Fenring", 6));
				players.back()->addLeader(Leader("Captain Aramsham", 5));
				players.back()->addLeader(Leader("Caid", 3));
				players.back()->addLeader(Leader("Burseg", 3));
				players.back()->addLeader(Leader("Bashar", 2));
				break;
			case (int)faction::SPACING_GUILD:
				players.back()->addLeader(Leader("Staban Tuek", 5));
				players.back()->addLeader(Leader("Master Bewt", 3));
				players.back()->addLeader(Leader("Esmar Tuek", 3));
				players.back()->addLeader(Leader("Soo-Soo Sook", 2));
				players.back()->addLeader(Leader("Guild Rep", 1));
				break;
			case (int)faction::BENE_GESSERIT:
				players.back()->addLeader(Leader("Alia", 5));
				players.back()->addLeader(Leader("Margot Lady Fenring", 5));
				players.back()->addLeader(Leader("Mother Ramallo", 5));
				players.back()->addLeader(Leader("Princess Irulan", 5));
				players.back()->addLeader(Leader("Wanna Yueh", 5));
				break;
		}
		
		// Assign faction ability
		switch (i) {
			case (int)faction::ATREIDES: players.back()->setFactionAbility(std::make_unique<AtreidesAbility>()); break;
			case (int)faction::HARKONNEN: players.back()->setFactionAbility(std::make_unique<HarkonnenAbility>()); break;
			case (int)faction::FREMEN: players.back()->setFactionAbility(std::make_unique<FremenAbility>()); break;
			case (int)faction::EMPEROR: players.back()->setFactionAbility(std::make_unique<EmperorAbility>()); break;
			case (int)faction::SPACING_GUILD: players.back()->setFactionAbility(std::make_unique<SpacingGuildAbility>()); break;
			case (int)faction::BENE_GESSERIT: players.back()->setFactionAbility(std::make_unique<BeneGesseritAbility>()); break;
		}
		
		// Call faction setup for starting spice/units customization
		if (players.back()->getFactionAbility()) {
			players.back()->getFactionAbility()->setupAtStart(players.back());
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
	if (eventLogger) {
		eventLogger->logDebug("=== Initializing Dune Game ===");
		eventLogger->logDebug("Number of players: " + std::to_string(playerCount));
		eventLogger->logDebug("Loaded settings:");
		eventLogger->logDebug("  advancedFactionAbilities: " + std::string(featureSettings.advancedFactionAbilities ? "ON" : "OFF"));
		eventLogger->logDebug("  increasedSpiceFlow: " + std::string(featureSettings.increasedSpiceFlow ? "ON" : "OFF"));
		eventLogger->logDebug("  advancedCombat: " + std::string(featureSettings.advancedCombat ? "ON" : "OFF"));
		eventLogger->logDebug("  advancedDoubleSpiceBlow: " + std::string(featureSettings.advancedDoubleSpiceBlow ? "ON" : "OFF"));
	}
	
	_map.initializeMap();
	if (eventLogger) {
		eventLogger->logDebug("Map initialized with " + std::to_string(_map.getTerritories().size()) + " territories");
	}
	spiceDeck.initialize(_map);
	spiceDeck.setUseExtendedSpiceBlow(featureSettings.advancedDoubleSpiceBlow);
	if (eventLogger && featureSettings.advancedDoubleSpiceBlow) {
		eventLogger->logDebug("[Features] Advanced Double Spice Blow enabled");
	}
	treacheryDeck.initialize();
	traitorDeck.initialize();
	if (eventLogger) {
		eventLogger->logDebug("Treachery deck initialized with " + std::to_string(treacheryDeck.getTotalCards()) + " cards");
	}

	// Deal starting treachery cards face-down.
	for (int i = 0; i < playerCount; ++i) {
		FactionAbility* ability = players[i]->getFactionAbility();
		int startingCount = ability ? ability->getStartingTreacheryCardCount() : 1;
		for (int draw = 0; draw < startingCount; ++draw) {
			treacheryCard card = treacheryDeck.drawCard();
			players[i]->addTreacheryCard(card.name);
		}

		if (eventLogger) {
			eventLogger->logDebug(players[i]->getFactionName() +
				" receives " + std::to_string(startingCount) + " starting treachery card(s) face down");
		}
	}
	
	stormSector = 0;
	initializeStormDeck();
	if (eventLogger) {
		eventLogger->logDebug("Storm will be placed randomly during Turn 1 STORM phase");
	}
	
	std::uniform_int_distribution<> offsetDist(0, NUM_TOKEN_SECTORS - 1);
	int randomOffset = offsetDist(rng);
	
	playerTokenSectors.clear();
	int spacing = NUM_TOKEN_SECTORS / playerCount;
	for (int i = 0; i < playerCount; ++i) {
		int index = (randomOffset + i * spacing) % NUM_TOKEN_SECTORS;
		playerTokenSectors.push_back(TOKEN_SECTORS[index]);
	}
	std::string sectorInfo = "Player tokens placed at sectors:";
	for (int i = 0; i < playerCount; ++i) {
		sectorInfo += " P" + std::to_string(i + 1) + "-" + std::to_string(playerTokenSectors[i]);
	}
	if (eventLogger) {
		eventLogger->logDebug(sectorInfo);
	}
	
	// Initialize turn order (will be recalculated after each storm phase)
	setTurnOrder();
	
	// Place starting forces for factions with custom deployments
	PhaseContext tempCtx(turnNumber, currentPhase, players, playerCount, _map,
		stormSector, lastStormCard, nextStormCard, hasNextStormCard, stormDeck,
		spiceDeck, treacheryDeck, traitorDeck, turnOrder, beneGesseritCharity, rng,
		interactiveMode, eventLogger.get(), interactionAdapter.get(),
		&reactionEngine, featureSettings);
	for (int i = 0; i < playerCount; ++i) {
		if (players[i]->getFactionAbility()) {
			players[i]->getFactionAbility()->placeStartingForces(tempCtx);
		}
	}
	
	if (eventLogger) {
		eventLogger->logDebug("=== Players Initialized ===");
	}
	for (int i = 0; i < playerCount; ++i) {
		if (eventLogger) {
			eventLogger->logDebug(players[i]->getFactionName() + ": " + 
				std::to_string(players[i]->getSpice()) + " spice, " +
				std::to_string(players[i]->getUnitsReserve()) + " units in reserve");
		}
		
		// Log leader info
		const auto& aliveLeaders = players[i]->getAliveLeaders();
		if (!aliveLeaders.empty() && eventLogger) {
			std::string leaders = "  Leaders: ";
			for (size_t j = 0; j < aliveLeaders.size(); j++) {
				leaders += aliveLeaders[j].name + " (power:" + std::to_string(aliveLeaders[j].power) + ")";
				if (j < aliveLeaders.size() - 1) leaders += ", ";
			}
			eventLogger->logDebug(leaders);
		}
	}
	
	if (eventLogger) {
		eventLogger->logDebug("=== Game Initialized ===");
	}
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
	phases[static_cast<int>(gamePhase::BATTLE)]         = std::make_unique<BattlePhase>();
	phases[static_cast<int>(gamePhase::SPICE_COLLECTION)] = std::make_unique<SpiceCollectionPhase>();
	phases[static_cast<int>(gamePhase::MENTAT_PAUSE)]   = std::make_unique<MentatPausePhase>();
}

void Game::initializeStormDeck() {
	stormDeck = {1, 2, 3, 4, 5, 6};
	std::shuffle(stormDeck.begin(), stormDeck.end(), rng);
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
		int distance = (tokenSector - stormSector + TOTAL_SECTORS) % TOTAL_SECTORS;
		playersWithSectors.push_back({i, distance});
	}
	
	// Sort by distance (smallest distance = closest counter-clockwise = first in turn order)
	std::sort(playersWithSectors.begin(), playersWithSectors.end(),
		[](const auto& a, const auto& b) { return a.second < b.second; });
	
	// Build turn order from sorted list
	for (const auto& pair : playersWithSectors) {
		turnOrder.push_back(pair.first);
	}
	
	// Log the calculated turn order
	if (eventLogger) {
		std::string turnOrderStr = "Turn Order (from Storm at Sector " + std::to_string(stormSector) + "): ";
		for (size_t i = 0; i < turnOrder.size(); ++i) {
			turnOrderStr += players[turnOrder[i]]->getFactionName();
			if (i < turnOrder.size() - 1) turnOrderStr += " -> ";
		}
		eventLogger->logDebug(turnOrderStr);
	}
}

bool Game::checkVictory() {
	for (int i = 0; i < playerCount; ++i) {
		if (_map.countControlledTerritories(i) >= 3) {
			if (eventLogger) {
				Event e(EventType::GAME_ENDED,
					"VICTORY! " + players[i]->getFactionName() + " controls 3 territories!",
					turnNumber, "");
				e.playerFaction = players[i]->getFactionName();
				eventLogger->logEvent(e);
			}
			return true;
		}
	}
	return false;
}

void Game::processPhase() {
	PhaseContext ctx(
		turnNumber, currentPhase, players, playerCount, _map,
		stormSector, lastStormCard, nextStormCard, hasNextStormCard,
		stormDeck, spiceDeck,
		treacheryDeck, traitorDeck, turnOrder, beneGesseritCharity, rng, interactiveMode,
		eventLogger.get(), interactionAdapter.get(), &reactionEngine, featureSettings
	);

	if (phases[static_cast<int>(currentPhase)]) {
		phases[static_cast<int>(currentPhase)]->execute(ctx);
	} else if (eventLogger) {
		eventLogger->logDebug(getPhaseName(currentPhase) + " Phase (TODO)");
	}

	// AnytimeSafe checkpoint: end-of-phase boundary. Tleilaxu Ghola and
	// other "play at any time" cards can fire here. The engine is a no-op
	// when no player holds such a card.
	reactionEngine.dispatchAnytimeSafe(ctx, "end of " + getPhaseName(currentPhase));

	// Capture gameEnded flag from phase context
	gameEnded = ctx.gameEnded;
	
	// Update turn order after storm phase
	if (currentPhase == gamePhase::STORM) {
		setTurnOrder();
	}
}

void Game::processTurn() {
	turnNumber++;
	if (eventLogger) {
		eventLogger->logDebug("--- Turn " + std::to_string(turnNumber) + " ---");
	}
	
	// Reset leader battle status at start of turn
	for (int i = 0; i < playerCount; ++i) {
		players[i]->resetLeaderBattleStatus();
	}
	
	// Log faction leaders at start of turn
	if (eventLogger) {
		eventLogger->logDebug("Leaders at start of turn:");
		for (int i = 0; i < playerCount; ++i) {
			const auto& aliveLeaders = players[i]->getAliveLeaders();
			if (!aliveLeaders.empty()) {
				std::string leaderStr = players[i]->getFactionName() + ": ";
				for (size_t j = 0; j < aliveLeaders.size(); j++) {
					leaderStr += aliveLeaders[j].name + " (power:" + std::to_string(aliveLeaders[j].power) + ")";
					if (j < aliveLeaders.size() - 1) leaderStr += ", ";
				}
				eventLogger->logDebug(leaderStr);
			}
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
	
	while (turnNumber < MAX_TURNS && !gameEnded) {
		processTurn();
		
		if (eventLogger) {
			eventLogger->logDebug("");
		}
	}
	
	if (turnNumber >= MAX_TURNS && !gameEnded) {
		PhaseContext ctx(
			turnNumber, currentPhase, players, playerCount, _map,
			stormSector, lastStormCard, nextStormCard, hasNextStormCard,
			stormDeck, spiceDeck,
			treacheryDeck, traitorDeck, turnOrder, beneGesseritCharity, rng, interactiveMode,
			eventLogger.get(), interactionAdapter.get(), &reactionEngine, featureSettings
		);

		for (int i = 0; i < playerCount; ++i) {
			FactionAbility* ability = players[i]->getFactionAbility();
			if (ability && ability->checkSpecialVictory(ctx)) {
				gameEnded = true;
				if (eventLogger) {
					Event e(EventType::GAME_ENDED,
						"SPECIAL VICTORY! " + players[i]->getFactionName() +
						" wins because no faction claimed victory before turn limit!",
						turnNumber, "");
					e.playerFaction = players[i]->getFactionName();
					eventLogger->logEvent(e);
				}
				break;
			}
		}

		if (!gameEnded && eventLogger) {
			eventLogger->logDebug("=== GAME ENDED: Max turns reached ===");
		}
	}
}

