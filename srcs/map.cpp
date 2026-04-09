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
	// For old calls: store spice in first sector (or sector -1 for no spice)
	if (spiceAmount > 0 && !sectors.empty()) {
		newTerritory.spicePresent.push_back({spiceAmount, sectors[0]});
	}
	newTerritory.sectors = sectors;
	newTerritory.hasTransporter = false;
	newTerritory.specialMovement = false;
	territories.push_back(newTerritory);
}

void GameMap::addTerritory(const std::string& name, terrainType terrain, int spiceAmount, const std::vector<int>& sectors, const int spiceSector) {
	territory newTerritory;
	newTerritory.name = name;
	newTerritory.terrain = terrain;
	// New version: store spice with explicit sector
	if (spiceAmount > 0) {
		newTerritory.spicePresent.push_back({spiceAmount, spiceSector});
	}
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
	addTerritory("Cielago North",      terrainType::desert, 8,  {1, 2, 3}, 3);
	addTerritory("Cielago South",      terrainType::desert, 12, {2, 3}, 2);
	addTerritory("Cielago East",       terrainType::desert, 0,  {3, 4});
	addTerritory("Harg Pass",          terrainType::desert,  0, {4, 5});
	addTerritory("South Mesa",         terrainType::desert,  10, {4, 5, 6}, 5);
	addTerritory("The Minor Erg",      terrainType::desert,  8, {5, 6, 7, 8}, 8);
	addTerritory("Red Chasm",          terrainType::desert,  8, {7}, 7);
	addTerritory("Gara Kulon",         terrainType::desert,  0, {8});
	addTerritory("Basin",              terrainType::desert,  0, {9});
	addTerritory("Hole in the Rock",   terrainType::desert,  0, {9});
	addTerritory("Sihaya Ridge",       terrainType::desert,  6, {9}, 9);
	addTerritory("Imperial Basin",     terrainType::desert,  0, {9, 10, 11});
	addTerritory("Old Gap",            terrainType::desert,  6, {9, 10, 11}, 10);
	addTerritory("Arsunt",             terrainType::desert,  0, {11, 12});
	addTerritory("Broken Land",        terrainType::desert,  8, {11, 12}, 12);
	addTerritory("Tsimpo",             terrainType::desert,  0, {11, 12, 13});
	addTerritory("Hagga Basin",        terrainType::desert,  6, {12, 13}, 13);
	addTerritory("Rock Outcroppings",  terrainType::desert,  6, {13, 14}, 14);
	addTerritory("Bight of the Cliff", terrainType::desert,  0, {14, 15});
	addTerritory("Wind Pass",          terrainType::desert,  0, {14, 15, 16, 17});
	addTerritory("The Great Flat",     terrainType::desert, 10, {15}, 15);
	addTerritory("Funeral Plain",      terrainType::desert,  6, {15}, 15);
	addTerritory("The Greater Flat",   terrainType::desert,  0, {16});
	addTerritory("Habbanya Erg",       terrainType::desert,  8, {16, 17}, 16);
	addTerritory("Wind Pass North",    terrainType::desert,  6, {17, 18}, 17);
	addTerritory("Habbanya Ridge Flat",terrainType::desert, 10, {17, 18}, 18);

	addTerritory("Polar Sink", terrainType::northPole, 0,
		{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18});

	linkNeighbours();
}

// =============================================================================
// Storm helpers
// =============================================================================

std::set<int> GameMap::getStormSweep(int startSector, int move) {
	std::set<int> swept;
	for (int i = 1; i <= move; ++i) {
		int sector = ((startSector - 1 + i) % TOTAL_SECTORS) + 1;
		swept.insert(sector);
	}
	return swept;
}

bool GameMap::canLeaveSector(int unitSector, int stormSector) {
	return unitSector != stormSector;
}

bool GameMap::canEnterTerritory(const territory* dest, int stormSector) {
	if (dest == nullptr) return false;
	if (dest->terrain == terrainType::northPole) return true;
	if (dest->terrain == terrainType::rock)      return true;
	if (dest->terrain == terrainType::city)      return true;
	for (int s : dest->sectors) {
		if (s != stormSector) return true;
	}
	return false;
}

int GameMap::firstSafeSector(const territory* dest, int stormSector) {
	if (dest == nullptr) return -1;
	if (dest->terrain != terrainType::desert) {
		return dest->sectors.empty() ? -1 : dest->sectors[0];
	}
	for (int s : dest->sectors) {
		if (s != stormSector) return s;
	}
	return -1;
}

// =============================================================================
// Unit operations — sector-aware
// =============================================================================

void GameMap::addUnitsToTerritory(const std::string& territoryName, int factionIndex,
                                   int normalUnits, int eliteUnits, int sector) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	int effectiveSector = sector;
	if (effectiveSector == -1) {
		effectiveSector = terr->sectors.empty() ? 1 : terr->sectors[0];
	}

	for (auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex && stack.sector == effectiveSector) {
			stack.normal_units += normalUnits;
			stack.elite_units  += eliteUnits;
			return;
		}
	}

	unitStack newStack;
	newStack.factionOwner  = factionIndex;
	newStack.normal_units  = normalUnits;
	newStack.elite_units   = eliteUnits;
	newStack.sector        = effectiveSector;
	terr->unitsPresent.push_back(newStack);
}

void GameMap::removeUnitsFromTerritorySector(const std::string& territoryName, int factionIndex,
                                              int normalUnits, int eliteUnits, int sector) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	for (auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex && stack.sector == sector) {
			stack.normal_units = std::max(0, stack.normal_units - normalUnits);
			stack.elite_units  = std::max(0, stack.elite_units  - eliteUnits);
			break;
		}
	}

	terr->unitsPresent.erase(
		std::remove_if(terr->unitsPresent.begin(), terr->unitsPresent.end(),
			[](const unitStack& s) { return s.normal_units == 0 && s.elite_units == 0; }),
		terr->unitsPresent.end());
}

void GameMap::removeUnitsFromTerritory(const std::string& territoryName, int factionIndex,
                                        int normalUnits, int eliteUnits) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	int normalLeft = normalUnits;
	int eliteLeft  = eliteUnits;

	for (auto& stack : terr->unitsPresent) {
		if (stack.factionOwner != factionIndex) continue;
		if (normalLeft <= 0 && eliteLeft <= 0) break;

		int takeNormal = std::min(normalLeft, stack.normal_units);
		int takeElite  = std::min(eliteLeft,  stack.elite_units);
		stack.normal_units -= takeNormal;
		stack.elite_units  -= takeElite;
		normalLeft         -= takeNormal;
		eliteLeft          -= takeElite;
	}

	terr->unitsPresent.erase(
		std::remove_if(terr->unitsPresent.begin(), terr->unitsPresent.end(),
			[](const unitStack& s) { return s.normal_units == 0 && s.elite_units == 0; }),
		terr->unitsPresent.end());
}

int GameMap::getUnitsInTerritory(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	int total = 0;
	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) {
			total += stack.normal_units + stack.elite_units;
		}
	}
	return total;
}

int GameMap::getUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex && stack.sector == sector) {
			return stack.normal_units + stack.elite_units;
		}
	}
	return 0;
}

int GameMap::getEliteUnitsInTerritorySector(const std::string& territoryName, int factionIndex, int sector) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex && stack.sector == sector) {
			return stack.elite_units;
		}
	}
	return 0;
}

// =============================================================================
// Spice operations — sector-aware
// =============================================================================

void GameMap::addSpiceToTerritory(const std::string& territoryName, int amount, int sector) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr || amount <= 0) return;

	// Check if spice already exists in this sector
	for (auto& stack : terr->spicePresent) {
		if (stack.sector == sector) {
			stack.amount += amount;
			return;
		}
	}

	// Create new spice stack in this sector
	spiceStack newStack;
	newStack.amount = amount;
	newStack.sector = sector;
	terr->spicePresent.push_back(newStack);
}

int GameMap::getSpiceInTerritory(const std::string& territoryName) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	int total = 0;
	for (const auto& stack : terr->spicePresent) {
		total += stack.amount;
	}
	return total;
}

int GameMap::getSpiceInSector(const std::string& territoryName, int sector) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	for (const auto& stack : terr->spicePresent) {
		if (stack.sector == sector) {
			return stack.amount;
		}
	}
	return 0;
}

int GameMap::removeSpiceFromSector(const std::string& territoryName, int amount, int sector) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return 0;

	for (auto& stack : terr->spicePresent) {
		if (stack.sector == sector) {
			int removed = std::min(amount, stack.amount);
			stack.amount -= removed;
			// Don't remove empty stacks — just leave them at 0
			// (simplifies iteration elsewhere)
			return removed;
		}
	}
	return 0;
}

void GameMap::removeAllSpiceFromTerritory(const std::string& territoryName) {
	territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return;

	terr->spicePresent.clear();
}

// =============================================================================
// Neighbour links
// =============================================================================

void GameMap::linkNeighbours() {
	// CITIES
	linkTerritoryNeighbours("Arrakeen", {
		"Rim Wall West", "Imperial Basin", 
		"Old Gap"
	});
	linkTerritoryNeighbours("Carthag", {
		"Arsunt", "Imperial Basin", 
		"Tsimpo", "Hagga Basin"
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

	// ROCK
	linkTerritoryNeighbours("Shield Wall", {
		"Hole in the Rock", "Gara Kulon",
		"False Wall East", "Imperial Basin",
		"Sihaya Ridge", "Pasty Mesa",
		"The Minor Erg"
	});
	linkTerritoryNeighbours("Rim Wall West", {
		"Arrakeen", "Imperial Basin",
		"Basin", "Hole in the Rock",
		"Old Gap"
	});
	linkTerritoryNeighbours("False Wall East", {
		"Shield Wall", "The Minor Erg", 
		"Imperial Basin", "Harg Pass",
		"Polar Sink"
	});
	linkTerritoryNeighbours("False Wall South", {
		"Tuek's Sietch", "Pasty Mesa", 
		"South Mesa", "Harg Pass",
		"The Minor Erg", "Cielago East",
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
		"Funeral Plain", "The Great Flat",
		"Broken Land"
	});
	linkTerritoryNeighbours("False Wall West", {
		"Wind Pass", "Habbanya Erg",
		"The Greater Flat", "Habbanya Ridge Flat",
		"Cielago West"
	});

	// DESERT
	linkTerritoryNeighbours("Cielago West", {
		"Habbanya Ridge Flat", "Cielago North", 
		"Meridian", "Wind Pass North",
		"Wind Pass", "False Wall West",
		"Cielago Depression"
	});
	linkTerritoryNeighbours("Meridian", {
		"Cielago West", "Cielago South",
		"Habbanya Ridge Flat", "Cielago Depression"
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
		"False Wall South", "False Wall East",
		"Cielago North", "The Minor Erg",
		"Polar Sink"
	});
	linkTerritoryNeighbours("South Mesa", {
		"Tuek's Sietch", "False Wall South", 
		"Cielago East", "Pasty Mesa",
		"Red Chasm"
	});
	linkTerritoryNeighbours("The Minor Erg", {
		"False Wall South", "False Wall East",
		"Harg Pass", "Pasty Mesa",
		"Shield Wall"
	});
	linkTerritoryNeighbours("Red Chasm", {
		"Pasty Mesa", "South Mesa"
	});
	linkTerritoryNeighbours("Gara Kulon", {
		"Shield Wall", "Sihaya Ridge",
		"Pasty Mesa"
	});
	linkTerritoryNeighbours("Basin", {
		"Sihaya Ridge", "Rim Wall West",
		"Old Gap", "Hole in the Rock"
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
		"Plastic Basin", "Rock Outcroppings"
	});
	linkTerritoryNeighbours("Tsimpo", {
		"Carthag", "Plastic Basin", 
		"Broken Land", "Hagga Basin",
		"Imperial Basin", "Old Gap"
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
		"Rock Outcroppings", "Plastic Basin"
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
		"False Wall West", "Habbanya Erg"
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

	// POLAR SINK
	linkTerritoryNeighbours("Polar Sink", {
		"False Wall East", "Harg Pass", 
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

// =============================================================================
// Control / occupancy queries
// =============================================================================

int GameMap::countControlledTerritories(int factionIndex) const {
	int count = 0;
	for (const auto& terr : territories) {
		if (isControlled(terr.name, factionIndex)) count++;
	}
	return count;
}

bool GameMap::isControlled(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return false;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex &&
		    (stack.normal_units > 0 || stack.elite_units > 0)) {
			return true;
		}
	}
	return false;
}

int GameMap::getControllingFaction(const std::string& territoryName) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return -1;

	int maxUnits = 0;
	int controllingFaction = -1;

	for (const auto& stack : terr->unitsPresent) {
		int total = stack.normal_units + stack.elite_units;
		if (total > maxUnits) {
			maxUnits = total;
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
	return static_cast<int>(factionsPresent.size());
}

bool GameMap::canAddFactionToTerritory(const std::string& territoryName, int factionIndex) const {
	const territory* terr = getTerritory(territoryName);
	if (terr == nullptr) return false;

	if (terr->terrain == terrainType::northPole) return true;

	for (const auto& stack : terr->unitsPresent) {
		if (stack.factionOwner == factionIndex) return true;
	}

	return countFactionsInTerritory(territoryName) < 2;
}

std::vector<std::string> GameMap::getTerritoriesWithUnits(int factionIndex) const {
	std::vector<std::string> result;
	for (const auto& terr : territories) {
		if (getUnitsInTerritory(terr.name, factionIndex) > 0) {
			result.push_back(terr.name);
		}
	}
	return result;
}

// =============================================================================
// Territory access
// =============================================================================

territory* GameMap::getTerritory(const std::string& name) {
	for (auto& terr : territories) {
		if (terr.name == name) return &terr;
	}
	return nullptr;
}

const territory* GameMap::getTerritory(const std::string& name) const {
	for (const auto& terr : territories) {
		if (terr.name == name) return &terr;
	}
	return nullptr;
}

const std::vector<territory>& GameMap::getTerritories() const {
	return territories;
}
