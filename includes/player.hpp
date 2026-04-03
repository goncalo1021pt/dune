#pragma once

#include <vector>
#include <string>

#define MAX_TREACHERY_CARDS 4
#define MAX_TRAITOR_CARDS 4

// Forward declaration
enum class faction;

class Player {
	private:
		std::string factionName;
		int factionIndex;
		int homeSectorIndex;	

		int spice;

		int unitsReserve;
		int unitsDeployed;
		int unitsDestroyed;

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
		int getUnitsDeployed() const;
		int getUnitsDestroyed() const;
		int getTotalUnits() const;

		// Resource management
		void addSpice(int amount);
		void removeSpice(int amount);
		void deployUnits(int count);
		void recallUnits(int count);
		void destroyUnits(int count);
		void reviveUnits(int count);

		// Card management
		void addTreacheryCard(const std::string& card);
		void removeTreacheryCard(const std::string& card);
		const std::vector<std::string>& getTreacheryCards() const;

		void addTraitorCard(const std::string& card);
		void removeTraitorCard(const std::string& card);
		const std::vector<std::string>& getTraitorCards() const;
};
