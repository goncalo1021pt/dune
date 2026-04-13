#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "settings.hpp"


enum class faction;

struct unitStack {
	int factionOwner;		// faction index (0-5)
	int normal_units;
	int elite_units; 		// Fedaykin or Sardaukar
	bool advisor;			// Bene Gesserit peaceful advisor state
	int sector;				// which sector of the territory these units occupy
							// matches one entry from the territory's sectors[] list
};

struct spiceStack {
	int amount;
	int sector;
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
	
	std::vector<spiceStack> spicePresent;	// spice piles with sector info
	std::vector<unitStack> unitsPresent;
	bool hasTransporter;  // if its arrakeen or carthag
	bool specialMovement;  // if its arrakeen or carthag
};


class GameMap {
	private:
		std::vector<territory> territories;
		
		void addTerritory(const std::string& name, terrainType terrain, int spiceAmount, const std::vector<int>& sectors);
		void addTerritory(const std::string& name, terrainType terrain, int spiceAmount, const std::vector<int>& sectors, const int spiceSector);
		void linkNeighbours();
		void linkTerritoryNeighbours(const std::string& territoryName, const std::vector<std::string>& neighbourNames);
		
	public:
		GameMap();
		~GameMap();

		void initializeMap();
		
		territory* getTerritory(const std::string& name);
		const territory* getTerritory(const std::string& name) const;
		const std::vector<territory>& getTerritories() const;

		// ---------------------------------------------------------------
		// Unit placement — sector-aware
		// sector: which sector of the territory units are placed in.
		//         Must be one of the values in territory.sectors[].
		//         Use territory.sectors[0] as the default for single-sector
		//         territories or when sector precision doesn't matter.
		// ---------------------------------------------------------------
		void addUnitsToTerritory(const std::string& territoryName, int factionIndex,
		                         int normalUnits, int eliteUnits, int sector,
		                         bool asAdvisor);
		void addUnitsToTerritory(const std::string& territoryName, int factionIndex,
		                         int normalUnits, int eliteUnits, int sector);

		// Remove units from a specific (faction, sector) stack.
		// Use when precise sector removal is needed (storm damage, worm).
		void removeUnitsFromTerritorySector(const std::string& territoryName, int factionIndex,
		                                    int normalUnits, int eliteUnits, int sector);
		void removeUnitsFromTerritorySector(const std::string& territoryName, int factionIndex,
		                                    int normalUnits, int eliteUnits, int sector,
		                                    bool fromAdvisorStack);

		// Remove units from a faction regardless of sector (battle losses,
		// general recall). Removes from stacks in insertion order.
		void removeUnitsFromTerritory(const std::string& territoryName, int factionIndex,
		                              int normalUnits, int eliteUnits);

		// Total units for a faction in the whole territory (all sectors summed).
		// Used by battle resolution, spice collection, control checks.
		int getUnitsInTerritory(const std::string& territoryName, int factionIndex) const;
		int getAdvisorUnitsInTerritory(const std::string& territoryName, int factionIndex) const;
		int getCombatUnitsInTerritory(const std::string& territoryName, int factionIndex) const;

		// Units for a faction in a specific sector only.
		int getUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const;
		int getAdvisorUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const;
		int getCombatUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const;

	// Get breakdown of unit types (for battle strength calculation)
	// Returns pair of (normal units, elite units) in territory
	std::pair<int, int> getUnitBreakdown(const std::string& territoryName, int factionIndex) const;
	std::pair<int, int> getCombatUnitBreakdown(const std::string& territoryName, int factionIndex) const;
		int getEliteUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const;
		int getAdvisorUnitsInTerritoryAllFactions(const std::string& territoryName) const;
		int flipFightersToAdvisors(const std::string& territoryName, int factionIndex, int count = -1);
		int flipAdvisorsToFighters(const std::string& territoryName, int factionIndex, int count = -1);

		// ---------------------------------------------------------------
		// Spice operations — sector-aware
		// ---------------------------------------------------------------

		// Add spice to a territory in a specific sector.
		void addSpiceToTerritory(const std::string& territoryName, int amount, int sector);

		// Get total spice in a territory (all sectors summed).
		int getSpiceInTerritory(const std::string& territoryName) const;

		// Get spice in a specific sector only.
		int getSpiceInSector(const std::string& territoryName, int sector) const;

		// Remove spice from a specific sector.
		// Returns amount actually removed (may be less than requested).
		int removeSpiceFromSector(const std::string& territoryName, int amount, int sector);

		// Remove all spice from a territory.
		void removeAllSpiceFromTerritory(const std::string& territoryName);

		// ---------------------------------------------------------------
		// Storm helpers
		// ---------------------------------------------------------------

		// Returns the set of all sector numbers the storm sweeps through
		// (inclusive of start+1 through start+move, wrapping at 18).
		// startSector: sector the storm occupied BEFORE this move.
		// move: number of sectors travelled this turn.
		static std::set<int> getStormSweep(int startSector, int move);

		// Returns true if a unit in `unitSector` can leave its territory
		// given the current storm position. Units cannot move out of a
		// sector that is currently under storm.
		static bool canLeaveSector(int unitSector, int stormSector);

		// Returns true if a territory has at least one sector not in storm,
		// meaning units can arrive there.
		static bool canEnterTerritory(const territory* dest, int stormSector);

		// Returns the first sector of a territory that is not in storm.
		// Returns -1 if the entire territory is in storm (should not happen
		// for valid destinations after canEnterTerritory check).
		static int firstSafeSector(const territory* dest, int stormSector);

		// ---------------------------------------------------------------
		// Control / occupancy queries (territory-level, sector-agnostic)
		// ---------------------------------------------------------------
		int countControlledTerritories(int factionIndex) const;
		bool isControlled(const std::string& territoryName, int factionIndex) const;
		int getControllingFaction(const std::string& territoryName) const;
		
		int countFactionsInTerritory(const std::string& territoryName) const;
		int countCombatFactionsInTerritory(const std::string& territoryName) const;
		bool canAddFactionToTerritory(const std::string& territoryName, int factionIndex) const;
		bool canAddAdvisorToTerritory(const std::string& territoryName, int factionIndex) const;
		
		// Utility methods
		std::vector<std::string> getTerritoriesWithUnits(int factionIndex) const;
};
