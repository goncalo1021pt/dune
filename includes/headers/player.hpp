#pragma once

#include <vector>
#include <string>
#include <memory>
#include "settings.hpp"
#include "leader.hpp"
#include "factions/faction_ability.hpp"

// Forward declarations
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

		std::vector<Leader> aliveLeaders;
		std::vector<Leader> deadLeaders;

		std::vector<std::string> treacheryCards;

		std::vector<std::string> traitorCards;
		std::vector<std::string> traitorsDiscarded;

		std::unique_ptr<FactionAbility> ability;

	public:
		Player(int factionIndex, const std::string& factionName);
		~Player();

		// Faction ability
		void setFactionAbility(std::unique_ptr<FactionAbility> a);
		FactionAbility* getFactionAbility() const;

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
		void setSpice(int amount);  // Set spice directly (for initialization)
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

		// Leader management
		void addLeader(const Leader& leader);
		void killLeader(size_t aliveIndex);
		void reviveLeader(size_t deadIndex);
		const std::vector<Leader>& getAliveLeaders() const;
		std::vector<Leader>& getAliveLeadersMutable();
		const std::vector<Leader>& getDeadLeaders() const;
		void markLeaderBattled(size_t aliveLeaderIndex);
		void resetLeaderBattleStatus();
};
