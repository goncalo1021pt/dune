#include "debug.hpp"
#include "game.hpp"
#include "player.hpp"
#include "map.hpp"
#include <iostream>
#include <iomanip>

Game* GameDebugger::gameInstance = nullptr;

void GameDebugger::setGameInstance(Game* game) {
	gameInstance = game;
}

void GameDebugger::printGameState() {
	if (!gameInstance) {
		std::cout << "\n[DEBUG] No game instance set\n" << std::endl;
		return;
	}

	std::cout << "\n";
	std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
	std::cout << "║               DUNE GAME STATE DEBUG OUTPUT                     ║\n";
	std::cout << "╚════════════════════════════════════════════════════════════════╝\n";

	// Game info
	std::cout << "\n[GAME INFO]\n";
	std::cout << "  Turn: " << gameInstance->getTurnNumber() << "\n";
	std::cout << "  Storm Sector: " << gameInstance->getStormSector() << "\n";
	std::cout << "  Next Storm Card: " << gameInstance->getNextStormCard() << "\n";

	// Turn order
	const auto& turnOrder = gameInstance->getTurnOrder();
	if (!turnOrder.empty()) {
		std::cout << "  Turn Order: ";
		for (size_t i = 0; i < turnOrder.size(); i++) {
			std::cout << gameInstance->getPlayer(turnOrder[i])->getFactionName();
			if (i < turnOrder.size() - 1) std::cout << " -> ";
		}
		std::cout << "\n";
	}

	// Player info
	std::cout << "\n[PLAYERS]\n";
	for (int i = 0; i < gameInstance->getPlayerCount(); i++) {
		const Player* p = gameInstance->getPlayer(i);
		const auto& territories = gameInstance->getTerritories();
		int deployedFighters = 0;
		int deployedAdvisors = 0;
		for (const auto& territory : territories) {
			for (const auto& stack : territory.unitsPresent) {
				if (stack.factionOwner != i) continue;
				int stackTotal = stack.normal_units + stack.elite_units;
				if (stack.advisor) {
					deployedAdvisors += stackTotal;
				} else {
					deployedFighters += stackTotal;
				}
			}
		}
		
		std::cout << "  " << std::left << std::setw(12) << p->getFactionName() << ": ";
		std::cout << std::right << std::setw(2) << p->getSpice() << " spice | ";
		std::cout << "Reserve: " << std::setw(2) << p->getUnitsReserve() << " units";
		
		if (p->getEliteUnitsReserve() > 0) {
			std::cout << " (" << p->getEliteUnitsReserve() << " elite)";
		}
		
		std::cout << " | Deployed: " << std::setw(2) << p->getUnitsDeployed();
		if (deployedAdvisors > 0 || p->getFactionName() == "Bene Gesserit") {
			std::cout << " [F:" << deployedFighters << " A:" << deployedAdvisors << "]";
		}
		std::cout << "\n";

		// Leaders
		const auto& aliveLeaders = p->getAliveLeaders();
		const auto& deadLeaders = p->getDeadLeaders();
		
		if (!aliveLeaders.empty()) {
			std::cout << "    Alive Leaders: ";
			for (size_t j = 0; j < aliveLeaders.size(); j++) {
				std::cout << aliveLeaders[j].name << " (power:" << aliveLeaders[j].power << ")";
				if (aliveLeaders[j].hasBattled) std::cout << "[battled]";
				if (j < aliveLeaders.size() - 1) std::cout << ", ";
			}
			std::cout << "\n";
		}
		
		if (!deadLeaders.empty()) {
			std::cout << "    Dead Leaders: ";
			for (size_t j = 0; j < deadLeaders.size(); j++) {
				std::cout << deadLeaders[j].name << " (power:" << deadLeaders[j].power << ")";
				if (j < deadLeaders.size() - 1) std::cout << ", ";
			}
			std::cout << "\n";
		}

		// Treachery cards
		const auto& treacheryCards = p->getTreacheryCards();
		if (!treacheryCards.empty()) {
			std::cout << "    Cards (" << treacheryCards.size() << "): ";
			for (size_t j = 0; j < treacheryCards.size(); j++) {
				std::cout << treacheryCards[j];
				if (j < treacheryCards.size() - 1) std::cout << ", ";
			}
			std::cout << "\n";
		}
	}

	// Territory info
	std::cout << "\n[TERRITORIES]\n";
	const auto& territories = gameInstance->getTerritories();
	
	for (const auto& territory : territories) {
		int totalSpice = 0;
		for (const auto& spice : territory.spicePresent) {
			totalSpice += spice.amount;
		}
		bool hasSpice = totalSpice > 0;
		bool hasTroops = !territory.unitsPresent.empty();
		
		if (hasSpice || hasTroops) {
			std::cout << "  " << std::setw(20) << territory.name << ": ";
			
			if (hasSpice) {
				std::cout << totalSpice << " spice";
				if (hasTroops) std::cout << " | ";
			}
			
			if (hasTroops) {
				bool first = true;
				for (const auto& stack : territory.unitsPresent) {
					if (stack.normal_units <= 0 && stack.elite_units <= 0) {
						continue;
					}
					if (!first) std::cout << ", ";
					
					// Get faction name using centralized utility
					std::string factionName = getFactionName(stack.factionOwner);
					
					std::cout << factionName << ": " << stack.normal_units;
					if (stack.elite_units > 0) {
						std::cout << " (" << stack.elite_units << "e)";
					}
					std::cout << (stack.advisor ? " [advisor]" : " [fighter]");
					first = false;
				}
			}
			std::cout << "\n";
		}
	}

	std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
	std::cout << "║                    END OF DEBUG OUTPUT                         ║\n";
	std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
}
