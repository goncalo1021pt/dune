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
	territories.push_back(newTerritory);
}

void GameMap::initializeMap() {
    territories.clear();

    //STRONGHOLDS (Cities)
    addTerritory("Arrakeen", terrainType::city, 0, {10});
    addTerritory("Carthag", terrainType::city, 0, {11, 12});
    addTerritory("Sietch Tabr", terrainType::city, 0, {14});
    addTerritory("Habbanya Sietch", terrainType::city, 0, {4});
    addTerritory("Tuek's Sietch", terrainType::city, 0, {1});

    //ROCK TERRITORIES (Storm Safe)
    addTerritory("Shield Wall", terrainType::rock, 0, {8, 9});
    addTerritory("False Wall East", terrainType::rock, 0, {7, 8});
    addTerritory("False Wall West", terrainType::rock, 0, {16, 17, 18});
    addTerritory("False Wall South", terrainType::rock, 0, {2, 3, 4});
    addTerritory("Rock Outcroppings", terrainType::rock, 0, {14, 15});
    addTerritory("Old Gap", terrainType::rock, 0, {12, 13});

    //DESERT (Spice Blow Locations)
	addTerritory("Meridian", terrainType::desert, 0, {1, 2});
	addTerritory("Cielago West", terrainType::desert, 0, {1, 18});
	addTerritory("Cielago Depression", terrainType::desert, 0, {1, 2, 3});
	addTerritory("Cielago North", terrainType::desert, 0, {1, 2, 3});
	addTerritory("Cielago South", terrainType::desert, 0, {1, 2, 3});

    addTerritory("Imperial Basin", terrainType::desert, 8, {10, 11, 12});
    addTerritory("Hagga Basin", terrainType::desert, 6, {13});
    addTerritory("The Great Flat", terrainType::desert, 10, {15, 16});
    addTerritory("Pasty Mesa", terrainType::desert, 0, {1, 2, 3, 9});
    addTerritory("Habbanya Erg", terrainType::desert, 8, {6});
    addTerritory("Cielago South", terrainType::desert, 12, {5, 6});

    //THE CENTER
    addTerritory("Polar Sink", terrainType::northPole, 0, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18});

    linkNeighbours();
}

void GameMap::linkNeighbours() {
    linkTerritoryNeighbours("Arrakeen", {"Imperial Basin", "Old Gap", "Shield Wall"});
    linkTerritoryNeighbours("Carthag", {"Imperial Basin", "Tsimpo", "Basin"});
    linkTerritoryNeighbours("Sietch Tabr", {"Broken Land", "Rock Outcroppings", "Plastic Basin"});
    linkTerritoryNeighbours("Tuek's Sietch", {"South Mesa", "Pasty Mesa", "False Wall South"});
    linkTerritoryNeighbours("Habbanya Sietch", {"False Wall South", "South Mesa", "Habbanya Ridge Flat"});

    linkTerritoryNeighbours("Shield Wall", {"Imperial Basin", "Pasty Mesa", "False Wall East", "Arrakeen"});
    linkTerritoryNeighbours("Imperial Basin", {"Arrakeen", "Carthag", "Shield Wall", "Hagga Basin", "Polar Sink"});
    linkTerritoryNeighbours("Polar Sink", {"Imperial Basin", "Hagga Basin", "False Wall West", "Habbanya Erg", "Cielago North"});
    
}

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
