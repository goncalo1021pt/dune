#include "map.hpp"
#include <algorithm>
#include <set>

GameMap::GameMap() {
}

GameMap::~GameMap() {
}

void GameMap::addTerritory(const std::string& name, terrainType terrain, int spiceAmount, const std::vector<int>& sectors) {
	territory newTerritory;
	newTerritory.name = name;
	newTerritory.terrain = terrain;
	newTerritory.spiceAmount = spiceAmount;
	newTerritory.sectors = sectors;
	newTerritory.hasTransporter = false;
	newTerritory.specialMovement = false;
	territories.push_back(newTerritory);
}

void GameMap::initializeMap() {
    territories.clear();

    addTerritory("Arrakeen",        terrainType::city, 0, {10});
    addTerritory("Carthag",         terrainType::city, 0, {11});
    addTerritory("Sietch Tabr",     terrainType::city, 0, {14});
    addTerritory("Habbanya Sietch", terrainType::city, 0, {17});
    addTerritory("Tuek's Sietch",   terrainType::city, 0, {5});

    getTerritory("Arrakeen")->hasTransporter  = true;
    getTerritory("Arrakeen")->specialMovement = true;
    getTerritory("Carthag")->hasTransporter   = true;
    getTerritory("Carthag")->specialMovement  = true;

    addTerritory("Shield Wall",      terrainType::rock, 0, {8, 9});
    addTerritory("Rim Wall West",    terrainType::rock, 0, {9}); 
    addTerritory("False Wall East",  terrainType::rock, 0, {5, 6, 7, 8, 9});
    addTerritory("False Wall South", terrainType::rock, 0, {4, 5});
    addTerritory("Pasty Mesa",       terrainType::rock, 0, {5, 6, 7, 8});
    addTerritory("Plastic Basin",    terrainType::rock, 0, {12, 13, 14});
    addTerritory("False Wall West",  terrainType::rock, 0, {16, 17, 18});

    addTerritory("Imperial Basin",  terrainType::desert,  0, {9, 10, 11});
    addTerritory("Arsunt",          terrainType::desert,  0, {11, 12});
    addTerritory("Tsimpo",          terrainType::desert,  0, {11, 12, 13});
    addTerritory("Hagga Basin",     terrainType::desert,  6, {12, 13});
    addTerritory("Basin",           terrainType::desert,  0, {9});
    addTerritory("Old Gap",         terrainType::desert,  6, {9, 10, 11});
    addTerritory("Broken Land",     terrainType::desert,  0, {11, 12});
    addTerritory("Sihaya Ridge",    terrainType::desert,  0, {9});
    addTerritory("Gara Kulon",      terrainType::desert,  0, {7});
    addTerritory("Hole in the Rock",terrainType::desert,  0, {9});
    addTerritory("The Minor Erg",   terrainType::desert,  0, {5, 6, 7, 8});
    addTerritory("Harg Pass",       terrainType::desert,  0, {4, 5});
    addTerritory("Red Chasm",       terrainType::desert,  8, {7});
    addTerritory("Rock Outcroppings",  terrainType::desert, 6,  {13, 14});
    addTerritory("Bight of the Cliff", terrainType::desert, 0,  {14, 15});
    addTerritory("Wind Pass",          terrainType::desert, 0,  {14, 15, 16, 17});
    addTerritory("Wind Pass North",    terrainType::desert, 6,  {17, 18});
    addTerritory("The Great Flat",     terrainType::desert, 10, {15});
    addTerritory("Funeral Plain",      terrainType::desert, 0,  {15});
    addTerritory("The Greater Flat",   terrainType::desert, 0,  {16});
    addTerritory("Habbanya Erg",       terrainType::desert, 8,  {16, 17});
    addTerritory("Habbanya Ridge Flat",terrainType::desert, 0,  {17, 18});
    addTerritory("Cielago North",      terrainType::desert, 8,  {1, 2, 3});
    addTerritory("Cielago West",       terrainType::desert, 0,  {18, 1});
    addTerritory("Cielago South",      terrainType::desert, 12, {2, 3});
    addTerritory("Cielago Depression", terrainType::desert, 0,  {1, 2, 3});
    addTerritory("Cielago East",       terrainType::desert, 0,  {3, 4});
    addTerritory("Meridian",           terrainType::desert, 0,  {1, 2});
    addTerritory("South Mesa",         terrainType::desert, 0,  {4, 5, 6});

    addTerritory("Polar Sink", terrainType::northPole, 0,
        {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18});

    linkNeighbours();
}

void GameMap::linkNeighbours() {

    // POLAR SINK touches everything on the inner ring
    linkTerritoryNeighbours("Polar Sink", {
        "Imperial Basin", "Arsunt", "Tsimpo", "Hagga Basin",
        "False Wall West", "Wind Pass North", "Wind Pass",
        "Harg Pass", "False Wall East", "The Minor Erg",
        "Pasty Mesa", "Cielago North", "Cielago West",
        "Cielago Depression", "Cielago South", "Meridian",
        "Funeral Plain", "The Greater Flat", "The Great Flat",
        "Habbanya Erg"
    });

    // --- STRONGHOLDS ---
    linkTerritoryNeighbours("Arrakeen", {
        "Imperial Basin", "Basin", "Rim Wall West",
        "Hole in the Rock", "Shield Wall", "Old Gap"
    });
    linkTerritoryNeighbours("Carthag", {
        "Imperial Basin", "Tsimpo", "Basin", "Old Gap"
    });
    linkTerritoryNeighbours("Sietch Tabr", {
        "Plastic Basin", "Rock Outcroppings", "Wind Pass", "Broken Land"
    });
    linkTerritoryNeighbours("Habbanya Sietch", {
        "Habbanya Ridge Flat", "Habbanya Erg", "False Wall West", "False Wall South"
    });
    linkTerritoryNeighbours("Tuek's Sietch", {
        "South Mesa", "False Wall South", "Pasty Mesa"
    });

    // --- ROCK TERRITORIES ---
    linkTerritoryNeighbours("Broken Land", {
        "Sietch Tabr", "Plastic Basin", "Hagga Basin", "Tsimpo"
    });
    linkTerritoryNeighbours("Tsimpo", {
        "Carthag", "Broken Land", "Hagga Basin", "Arsunt",
        "Imperial Basin", "Polar Sink"
    });
    linkTerritoryNeighbours("Basin", {
        "Arrakeen", "Carthag", "Old Gap", "Sihaya Ridge"
    });
    linkTerritoryNeighbours("Rim Wall West", {
        "Arrakeen", "Imperial Basin"
    });
    linkTerritoryNeighbours("Old Gap", {
        "Arrakeen", "Carthag", "Basin", "Broken Land"
    });
    linkTerritoryNeighbours("Sihaya Ridge", {
        "Basin", "Gara Kulon", "Shield Wall", "Hole in the Rock"
    });
    linkTerritoryNeighbours("Gara Kulon", {
        "Sihaya Ridge", "Shield Wall", "Pasty Mesa"
    });
    linkTerritoryNeighbours("Hole in the Rock", {
        "Arrakeen", "Sihaya Ridge", "Shield Wall"
    });
    linkTerritoryNeighbours("Shield Wall", {
        "Arrakeen", "Hole in the Rock", "Sihaya Ridge",
        "Gara Kulon", "False Wall East", "Imperial Basin"
    });
    linkTerritoryNeighbours("False Wall East", {
        "Shield Wall", "Pasty Mesa", "The Minor Erg", "Harg Pass", "Polar Sink"
    });
    linkTerritoryNeighbours("Pasty Mesa", {
        "Tuek's Sietch", "South Mesa", "False Wall South",
        "Gara Kulon", "Shield Wall", "False Wall East", "The Minor Erg", "Red Chasm"
    });
    linkTerritoryNeighbours("Red Chasm", {
        "Pasty Mesa", "The Minor Erg"
    });
    linkTerritoryNeighbours("South Mesa", {
        "Tuek's Sietch", "Habbanya Sietch", "False Wall South",
        "Cielago East", "Pasty Mesa"
    });
    linkTerritoryNeighbours("False Wall South", {
        "Tuek's Sietch", "Habbanya Sietch", "South Mesa",
        "Cielago East", "Cielago Depression"
    });
    linkTerritoryNeighbours("False Wall West", {
        "Habbanya Sietch", "Habbanya Erg", "Cielago North",
        "The Greater Flat", "Polar Sink"
    });
    linkTerritoryNeighbours("Plastic Basin", {
        "Sietch Tabr", "Broken Land", "Rock Outcroppings", "Wind Pass North"
    });
    linkTerritoryNeighbours("Rock Outcroppings", {
        "Sietch Tabr", "Plastic Basin", "Bight of the Cliff", "The Great Flat"
    });
    linkTerritoryNeighbours("Bight of the Cliff", {
        "Rock Outcroppings", "The Great Flat", "Funeral Plain"
    });
    linkTerritoryNeighbours("Habbanya Ridge Flat", {
        "Habbanya Sietch", "Habbanya Erg", "Cielago West", "Cielago Depression"
    });

    // --- DESERT TERRITORIES ---
    linkTerritoryNeighbours("Imperial Basin", {
        "Arrakeen", "Carthag", "Rim Wall West", "Shield Wall",
        "Tsimpo", "Arsunt", "Hagga Basin", "Polar Sink"
    });
    linkTerritoryNeighbours("Arsunt", {
        "Imperial Basin", "Tsimpo", "Hagga Basin", "Polar Sink"
    });
    linkTerritoryNeighbours("Hagga Basin", {
        "Arsunt", "Tsimpo", "Broken Land", "Plastic Basin",
        "Wind Pass North", "Imperial Basin", "Polar Sink"
    });
    linkTerritoryNeighbours("Wind Pass North", {
        "Hagga Basin", "Plastic Basin", "Wind Pass", "Polar Sink"
    });
    linkTerritoryNeighbours("Wind Pass", {
        "Wind Pass North", "Sietch Tabr", "Rock Outcroppings",
        "Funeral Plain", "Polar Sink"
    });
    linkTerritoryNeighbours("Funeral Plain", {
        "Wind Pass", "Bight of the Cliff", "The Great Flat", "Polar Sink"
    });
    linkTerritoryNeighbours("The Great Flat", {
        "Funeral Plain", "Rock Outcroppings", "Bight of the Cliff",
        "The Greater Flat", "Habbanya Erg", "Polar Sink"
    });
    linkTerritoryNeighbours("The Greater Flat", {
        "The Great Flat", "Habbanya Erg", "False Wall West",
        "Cielago North", "Polar Sink"
    });
    linkTerritoryNeighbours("Habbanya Erg", {
        "The Great Flat", "The Greater Flat", "False Wall West",
        "Habbanya Ridge Flat", "Habbanya Sietch", "Polar Sink"
    });
    linkTerritoryNeighbours("Cielago North", {
        "False Wall West", "The Greater Flat", "Cielago West",
        "Cielago Depression", "Polar Sink"
    });
    linkTerritoryNeighbours("Cielago West", {
        "Habbanya Ridge Flat", "Cielago North", "Cielago South",
        "Meridian", "Polar Sink"
    });
    linkTerritoryNeighbours("Cielago South", {
        "Cielago West", "Meridian", "Cielago Depression"
    });
    linkTerritoryNeighbours("Cielago Depression", {
        "Cielago North", "Cielago West", "Cielago South",
        "Cielago East", "False Wall South", "Habbanya Ridge Flat", "Polar Sink"
    });
    linkTerritoryNeighbours("Cielago East", {
        "Cielago Depression", "False Wall South", "South Mesa"
    });
    linkTerritoryNeighbours("Meridian", {
        "Cielago West", "Cielago South", "Polar Sink"
    });
    linkTerritoryNeighbours("The Minor Erg", {
        "False Wall East", "Red Chasm", "Pasty Mesa", "Harg Pass", "Polar Sink"
    });
    linkTerritoryNeighbours("Harg Pass", {
        "False Wall East", "The Minor Erg", "Polar Sink"
    });
}

// -----------------------------------------------------------------------------
// Everything below this line is unchanged from your original map.cpp
// -----------------------------------------------------------------------------

void GameMap::linkTerritoryNeighbours(const std::string& territoryName,
                                      const std::vector<std::string>& neighbourNames) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	terr->neighbourPtrs.clear();

	for (const auto& neighbourName : neighbourNames) {
		territory* neighbour = getTerritory(neighbourName);
		if (neighbour != nullptr) {
			terr->neighbourPtrs.push_back(neighbour);
		}
	}
}

territory* GameMap::getTerritory(const std::string& name) {
	for (auto& territory : territories) {
		if (territory.name == name) {
			return &territory;
		}
	}
	return nullptr;
}

const territory* GameMap::getTerritory(const std::string& name) const {
	for (const auto& territory : territories) {
		if (territory.name == name) {
			return &territory;
		}
	}
	return nullptr;
}

const std::vector<territory>& GameMap::getTerritories() const {
	return territories;
}

void GameMap::addUnitsToTerritory(const std::string& territoryName, int factionIndex, 
                               int normalUnits, int eliteUnits) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	for (auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) {
			stack.normal_units += normalUnits;
			stack.elite_units += eliteUnits;
			return;
		}
	}

	unitStack newStack;
	newStack.factionOwner = factionIndex;
	newStack.normal_units = normalUnits;
	newStack.elite_units = eliteUnits;
	terr->unitsPresent.push_back(newStack);
}

void GameMap::removeUnitsFromTerritory(const std::string& territoryName, int factionIndex, 
                                    int normalUnits, int eliteUnits) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	for (auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) {
			stack.normal_units = std::max(0, stack.normal_units - normalUnits);
			stack.elite_units = std::max(0, stack.elite_units - eliteUnits);

			if (stack.normal_units == 0 && stack.elite_units == 0) {
				auto it = std::find_if(terr->unitsPresent.begin(), terr->unitsPresent.end(), [factionIndex](const unitStack& us) {
					return us.factionOwner == factionIndex; 
				});
				if (it != terr->unitsPresent.end()) {
					terr->unitsPresent.erase(it);
				}
			}
			return;
		}
	}
}

int GameMap::getUnitsInTerritory(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(const_cast<std::string&>(territoryName));
	if (terr == nullptr) return 0;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) {
			return stack.normal_units + stack.elite_units;
		}
	}
	return 0;
}

int GameMap::countControlledTerritories(int factionIndex) const {
	int count = 0;
	for (const auto& terr : territories) {
		if (isControlled(terr.name, factionIndex)) {
			count++;
		}
	}
	return count;
}

bool GameMap::isControlled(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(const_cast<std::string&>(territoryName));
	if (terr == nullptr) return false;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex && (stack.normal_units > 0 || stack.elite_units > 0)) {
			return true;
		}
	}
	return false;
}

int GameMap::getControllingFaction(const std::string& territoryName) const {
	const territory* terr = getTerritory(const_cast<std::string&>(territoryName));
	if (terr == nullptr) return -1;

	int maxUnits = 0;
	int controllingFaction = -1;

	for (const auto& stack : terr->unitsPresent) {
		int totalUnits = stack.normal_units + stack.elite_units;
		if (totalUnits > maxUnits) {
			maxUnits = totalUnits;
			controllingFaction = stack.factionOwner;
		}
	}

	return controllingFaction;
}

int GameMap::countFactionsInTerritory(const std::string& territoryName) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	std::set<int> factionsPresent;
	for (const auto& stack : terr->unitsPresent) {
		if (stack.normal_units > 0 || stack.elite_units > 0) {
			factionsPresent.insert(stack.factionOwner);
		}
	}
	return factionsPresent.size();
}

bool GameMap::canAddFactionToTerritory(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return false;

	if (terr->terrain == terrainType::northPole) {
		return true;
	}

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) {
			return true;
		}
	}

	return countFactionsInTerritory(territoryName) < 2;
}
