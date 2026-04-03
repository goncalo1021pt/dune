#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>


enum class faction;

struct unitStack {
	int factionOwner;		// faction index (0-5)
	int normal_units;
	int elite_units; 		// Fedaykin or Sardaukar
};

enum class terrainType {
	desert,
	rock,
	city,
	northPole,
};

struct territory
{
	std::string name;
	terrainType terrain;
	std::vector<int> sectors;
	std::vector<territory*> neighbourPtrs;  // Pointers to neighbor territories
	
	int spiceAmount;
	std::vector<unitStack> unitsPresent;
	bool hasTransporter;  // if its arakin or carthag
	bool specialMovement;  // if its arakin or carthag
};


class GameMap {
	private:
		std::vector<territory> territories;
		
		void addTerritory(const std::string& name, terrainType terrain, int spiceAmount, const std::vector<int>& sectors);
		void linkNeighbours();
		void linkTerritoryNeighbours(const std::string& territoryName, const std::vector<std::string>& neighbourNames);
		
	public:
		GameMap();
		~GameMap();

		void initializeMap();
		
		territory* getTerritory(const std::string& name);
		const territory* getTerritory(const std::string& name) const;
		const std::vector<territory>& getTerritories() const;
		
		void addUnitsToTerritory(const std::string& territoryName, int factionIndex, int normalUnits, int eliteUnits);
		void removeUnitsFromTerritory(const std::string& territoryName, int factionIndex, int normalUnits, int eliteUnits);
		int getUnitsInTerritory(const std::string& territoryName, int factionIndex) const;
		
		int countControlledTerritories(int factionIndex) const;
		bool isControlled(const std::string& territoryName, int factionIndex) const;
		int getControllingFaction(const std::string& territoryName) const;
		
		int countFactionsInTerritory(const std::string& territoryName) const;
		bool canAddFactionToTerritory(const std::string& territoryName, int factionIndex) const;
};

