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
	
	addTerritory("Cielago West",       terrainType::desert, 0,  {18, 1});
	addTerritory("Meridian",           terrainType::desert, 0,  {1, 2});
	addTerritory("Cielago Depression", terrainType::desert, 0,  {1, 2, 3});
	addTerritory("Cielago North",      terrainType::desert, 8,  {1, 2, 3});
	addTerritory("Cielago South",      terrainType::desert, 12, {2, 3});
	addTerritory("Cielago East",       terrainType::desert, 0,  {3, 4});
	addTerritory("Harg Pass",          terrainType::desert,  0, {4, 5});
	addTerritory("South Mesa",         terrainType::desert, 0,  {4, 5, 6});
	addTerritory("The Minor Erg",      terrainType::desert,  0, {5, 6, 7, 8});
	addTerritory("Red Chasm",          terrainType::desert,  8, {7});
	addTerritory("Gara Kulon",         terrainType::desert,  0, {8});
	addTerritory("Basin",              terrainType::desert,  0, {9});
	addTerritory("Hole in the Rock",   terrainType::desert,  0, {9});
	addTerritory("Sihaya Ridge",       terrainType::desert,  0, {9});
	addTerritory("Imperial Basin",     terrainType::desert,  0, {9, 10, 11});
	addTerritory("Old Gap",            terrainType::desert,  6, {9, 10, 11});
	addTerritory("Arsunt",             terrainType::desert,  0, {11, 12});
	addTerritory("Broken Land",        terrainType::desert,  0, {11, 12});
	addTerritory("Tsimpo",             terrainType::desert,  0, {11, 12, 13});
	addTerritory("Hagga Basin",        terrainType::desert,  6, {12, 13});
	addTerritory("Rock Outcroppings",  terrainType::desert, 6,  {13, 14});
	addTerritory("Bight of the Cliff", terrainType::desert, 0,  {14, 15});
	addTerritory("Wind Pass",          terrainType::desert, 0,  {14, 15, 16, 17});
	addTerritory("The Great Flat",     terrainType::desert, 10, {15});
	addTerritory("Funeral Plain",      terrainType::desert, 0,  {15});
	addTerritory("The Greater Flat",   terrainType::desert, 0,  {16});
	addTerritory("Habbanya Erg",       terrainType::desert, 8,  {16, 17});
	addTerritory("Wind Pass North",    terrainType::desert, 6,  {17, 18});
	addTerritory("Habbanya Ridge Flat",terrainType::desert, 0,  {17, 18});

	addTerritory("Polar Sink", terrainType::northPole, 0,
		{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18});

	linkNeighbours();
}

void GameMap::linkNeighbours() {
	//CITIES
	linkTerritoryNeighbours("Arrakeen", {
		"Rim Wall West", "Imperial Basin", 
		"Old Gap"
	});
	linkTerritoryNeighbours("Carthag", {
		"Arsunt", "Imperial Basin", 
		"Tsimpo", "hagga Basin"
	});
	linkTerritoryNeighbours("Sietch Tabr", {
		"Plastic Basin", "Rock Outcroppings", 
		"Bight of the Cliff"
	});
	linkTerritoryNeighbours("Habbanya Sietch", {
		"Habbanya Ridge Flat"
	});
	linkTerritoryNeighbours("Tuek's Sietch", {
		"False Wall South", "South Mesa", 
		"Pasty Mesa"
	});

	//ROCK
	linkTerritoryNeighbours("Shield Wall", {
		"Hole in the Rock", "Gara Kulon",
		"False Wall East", "Imperial Basin"
		"Sihaya Ridge", "Pasty Mesa",
		"The Minor Erg"
	});
	linkTerritoryNeighbours("Rim Wall West", {
		"Arrakeen", "Imperial Basin",
		"Basin", "Hole in the Rock"
		"Old Gap"
	});
	linkTerritoryNeighbours("False Wall East", {
		"Shield Wall", "The Minor Erg", 
		"Imperial Basin", "Harg Pass"
		"Polar Sink"
	});
	linkTerritoryNeighbours("False Wall South", {
		"Tuek's Sietch", "Pasty Mesa", 
		"South Mesa", "Harg Pass",
		"The Minor Erg", "Cielago East"
		"Cielago North"
	});
	linkTerritoryNeighbours("Pasty Mesa", {
		"Tuek's Sietch", "False Wall South", 
		"Red Chasm", "Gara Kulon",
		"The Minor Erg", "Shield Wall",
		"South Mesa"
	});
	linkTerritoryNeighbours("Plastic Basin", {
		"Sietch Tabr", "Rock Outcroppings", 
		"Hagga Basin", "Tsimpo",
		"Wind Pass", "Bight of the Cliff",
		"Funeral Plain", "The Great Flat"
	});
	linkTerritoryNeighbours("False Wall West", {
		"Wind Pass", "Habbanya Erg",
		"The Greater Flat", "Habbanya Ridge Flat",
		"Cielago West"
	});


	//DESERT 
	linkTerritoryNeighbours("Cielago West", {
		"Habbanya Ridge Flat", "Cielago North", 
		"Meridian", "Wind Pass North",
		"Wind Pass", "Fasle Wall West"
		"Cielago Depression"
	});
	linkTerritoryNeighbours("Meridian", {
		"Cielago West", "Cielago South",
		"Habbanya Ridge Flat", "Cielago Depression",
	});
	linkTerritoryNeighbours("Cielago Depression", {
		"Cielago West", "Meridian",
		"Cielago North", "Cielago South",
		"Cielago East"
	});
	linkTerritoryNeighbours("Cielago North", {
		"Cielago West", "Cielago Depression",
		"Cielago East", "Wind Pass North",
		"Harg Pass", "False Wall South", 
		"Polar Sink"
	});
	linkTerritoryNeighbours("Cielago South", {
		"Cielago West", "Meridian", 
		"Cielago Depression"
	});
	linkTerritoryNeighbours("Cielago East", {
		"False Wall South", "Cielago Depression",
		"Cielago South", "South Mesa",
		"Cielago North"
	});
	linkTerritoryNeighbours("Harg Pass", {
		"False Wall South", "Fasle Wall East",
		"Cielago North", "The Minor Erg"
		"Polar Sink"
	});
	linkTerritoryNeighbours("South Mesa", {
		"Tuek's Sietch", "False Wall South", 
		"Cielago East", "Pasty Mesa",
		"Red Chasm"
	});
	linkTerritoryNeighbours("The Minor Erg", {
		"Fasle Wall South", "False Wall East",
		"Harg Pass", "Pasty Mesa",
		"Shield Wall"
	});
	linkTerritoryNeighbours("Red Chasm", {
		"Pasty Mesa", "South Mesa",
	});
	linkTerritoryNeighbours("Gara Kulon", {
		"Shield Wall", "Sihaya Ridge",
		"Pasty Mesa"
	});
	linkTerritoryNeighbours("Basin", {
		"Sihaya Ridge", "Rim Wall West",
		"Old Gap", "Hole in the Rock",
	});
	linkTerritoryNeighbours("Hole in the Rock", {
		"Shield Wall", "Rim Wall West",
		"Basin", "Sihaya Ridge",
		"Imperial Basin"
	});
	linkTerritoryNeighbours("Sihaya Ridge", {
		"Basin", "Hole in the Rock", 
		"Shield Wall", "Gara Kulon"
	});
	linkTerritoryNeighbours("Imperial Basin", {
		"Arrakeen", "Carthag",
		"Rim Wall West", "False Wall East",
		"Shield Wall", "Hole in the Rock",
		"Old Gap", "Arsunt", 
		"Tsimpo", "Polar Sink"
	});
	linkTerritoryNeighbours("Old Gap", {
		"Arrakeen", "Basin", 
		"Broken Land", "Rim Wall West",
		"Imperial Basin", "Tsimpo"
	});
	linkTerritoryNeighbours("Arsunt", {
		"Carthag", "Imperial Basin",
		"Hagga Basin", "Polar Sink"
	});
	linkTerritoryNeighbours("Broken Land", {
		"Old Gap", "Tsimpo",
		"Plastic Basin", "Rock Outcroppings",
	});
	linkTerritoryNeighbours("Tsimpo", {
		"Carthag", "Plastic Basin", 
		"Broken Land", "Hagga Basin",
		"Imperial Basin", "Old Gap",
	});
	linkTerritoryNeighbours("Hagga Basin", {
		"Carthag", "Plastic Basin", 
		"Tsimpo", "Wind Pass",
		"Polar Sink"
	});
	linkTerritoryNeighbours("Rock Outcroppings", {
		"Sietch Tabr", "Plastic Basin", 
		"Bight of the Cliff", "Broken Land"
	});
	linkTerritoryNeighbours("Bight of the Cliff", {
		"Sietch Tabr", "Funeral Plain",
		"Rock Outcroppings", "Plastic Basin",
	});
	linkTerritoryNeighbours("Wind Pass", {
		"Hagga Basin", "Plastic Basin",
		"The Great Flat", "The Greater Flat",
		"False Wall West", "Wind Pass North",
		"Cielago West", "Polar Sink"
	});
	linkTerritoryNeighbours("The Great Flat", {
		"Plastic Basin", "Wind Pass", 
		"Funeral Plain", "The Greater Flat"
	});
	linkTerritoryNeighbours("Funeral Plain", {
		"Plastic Basin", "Bight of the Cliff",
		"The Great Flat"
	});
	linkTerritoryNeighbours("The Greater Flat", {
		"The Great Flat", "Wind Pass",
		"False Wall West", "Habbanya Erg",
	});
	linkTerritoryNeighbours("Habbanya Erg", {
		"False Wall West", "The Greater Flat",
		"Habbanya Ridge Flat"
	});
	linkTerritoryNeighbours("Wind Pass North", {
		"Wind Pass", "Cielago West",
		"Cielago North", "Polar Sink"
	});
	linkTerritoryNeighbours("Habbanya Ridge Flat", {
		"Habbanya Sietch", "False Wall West",
		"Cielago West", "Habbanya Erg",
		"Meridian"
	});

	// --- POLAR SINK — borders all inner-ring territories ---
	linkTerritoryNeighbours("Polar Sink", {
		"Fasle Wall East", "Harg Pass", 
		"Cielago North", "Wind Pass North",
		"Imperial Basin", "Arsunt",
		"Hagga Basin", "Wind Pass"
	});
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
