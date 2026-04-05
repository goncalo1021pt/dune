#pragma once

#include <vector>
#include <string>
#include "settings.hpp"

// Forward declaration
enum class faction;

class Player {
	private:
		std::string factionName;
		int factionIndex;
		int homeSectorIndex;	

		int spice;

		int unitsReserve;
		int eliteUnitsReserve;
		int unitsDeployed;
		int unitsDestroyed;
		int freeReviveModifier;

		std::vector<std::string> treacheryCards;

		std::vector<std::string> traitorCards;
		std::vector<std::string> traitorsDiscarded;

	public:
		Player(int factionIndex, const std::string& factionName);
		~Player();

		// Getters
		int getFactionIndex() const;
		const std::string& getFactionName() const;
		int getHomeSector() const;
		int getSpice() const;
		int getUnitsReserve() const;
		int getEliteUnitsReserve() const;
		int getUnitsDeployed() const;
		int getUnitsDestroyed() const;
		int getTotalUnits() const;
		int getFreeRevivesPerTurn() const;

		// Resource management
		void addSpice(int amount);
		void removeSpice(int amount);
		void deployUnits(int count);
		void recallUnits(int count);
		void destroyUnits(int count);
		void reviveUnits(int count);
		void deployEliteUnits(int count);
		void recallEliteUnits(int count);
		void destroyEliteUnits(int count);
		void reviveEliteUnits(int count);	
		void setFreeReviveModifier(int modifier);

		// Card management
		void addTreacheryCard(const std::string& card);
		void removeTreacheryCard(const std::string& card);
		const std::vector<std::string>& getTreacheryCards() const;
		void addTraitorCard(const std::string& card);
		void removeTraitorCard(const std::string& card);
		const std::vector<std::string>& getTraitorCards() const;
};
