#include "player.hpp"
#include <algorithm>

Player::Player(int factionIdx, const std::string& factionName) 
	: factionName(factionName), factionIndex(factionIdx), homeSectorIndex(factionIdx),
	  spice(STARTING_SPICE), 
	  reserveUnits({STARTING_UNITS, STARTING_SPECIAL_UNITS}),
	  deployedUnits({0, 0}),
	  destroyedUnits({0, 0}),
	  freeReviveModifier(1), aliveLeaders(), deadLeaders() {
}

Player::~Player() {
}

void Player::setFactionAbility(std::unique_ptr<FactionAbility> a) {
	ability = std::move(a);
}

FactionAbility* Player::getFactionAbility() const {
	return ability.get();
}

void Player::addSpice(int amount) {
	if (amount < 0) 
		return;
	spice += amount;
}

void Player::removeSpice(int amount) {
	if (amount < 0) 
		return;
	spice = std::max(0, spice - amount);
}

void Player::setSpice(int amount) {
	if (amount < 0)
		return;
	spice = amount;
}

void Player::deployUnits(int count) {
	if (count < 0) 
		return;
	int canDeploy = std::min(count, reserveUnits.normal);
	reserveUnits.normal -= canDeploy;
	deployedUnits.normal += canDeploy;
}

void Player::recallUnits(int count) {
	if (count < 0) 
		return;
	int canRecall = std::min(count, deployedUnits.normal);
	deployedUnits.normal -= canRecall;
	reserveUnits.normal += canRecall;
}

void Player::destroyUnits(int count) {
	if (count < 0) 
		return;
	int aliveUnits = reserveUnits.normal + deployedUnits.normal;
	int canDestroy = std::min(count, aliveUnits);
	
	// Destroy deployed first, then reserve
	int destroyDeployed = std::min(canDestroy, deployedUnits.normal);
	int destroyReserve = canDestroy - destroyDeployed;
	
	deployedUnits.normal -= destroyDeployed;
	reserveUnits.normal -= destroyReserve;
	destroyedUnits.normal += canDestroy;
}

void Player::reviveUnits(int count) {
	if (count < 0) 
		return;
	int canRevive = std::min(count, destroyedUnits.normal);
	destroyedUnits.normal -= canRevive;
	reserveUnits.normal += canRevive;
}

void Player::deployEliteUnits(int count) {
	if (count < 0) 
		return;
	int canDeploy = std::min(count, reserveUnits.elite);
	reserveUnits.elite -= canDeploy;
	deployedUnits.elite += canDeploy;
}

void Player::recallEliteUnits(int count) {
	if (count < 0) 
		return;
	int canRecall = std::min(count, deployedUnits.elite);
	deployedUnits.elite -= canRecall;
	reserveUnits.elite += canRecall;
}

void Player::destroyEliteUnits(int count) {
	if (count < 0) 
		return;
	int aliveElite = reserveUnits.elite + deployedUnits.elite;
	int canDestroy = std::min(count, aliveElite);
	
	// Destroy deployed first, then reserve
	int destroyDeployed = std::min(canDestroy, deployedUnits.elite);
	int destroyReserve = canDestroy - destroyDeployed;
	
	deployedUnits.elite -= destroyDeployed;
	reserveUnits.elite -= destroyReserve;
	destroyedUnits.elite += canDestroy;
}

void Player::reviveEliteUnits(int count) {
	if (count < 0) 
		return;
	int canRevive = std::min(count, destroyedUnits.elite);
	destroyedUnits.elite -= canRevive;
	reserveUnits.elite += canRevive;
}

void Player::setUnitsReserve(int amount) {
	reserveUnits.normal = std::max(0, amount);
}

void Player::setEliteUnitsReserve(int amount) {
	reserveUnits.elite = std::max(0, amount);
}

void Player::setFreeReviveModifier(int modifier) {
	freeReviveModifier = modifier;
}

void Player::addTreacheryCard(const std::string& card) {
	treacheryCards.push_back(card);
}

void Player::removeTreacheryCard(const std::string& card) {
	auto it = std::find(treacheryCards.begin(), treacheryCards.end(), card);
	if (it != treacheryCards.end()) {
		treacheryCards.erase(it);
	}
}

void Player::addTraitorCard(const std::string& card) {
	traitorCards.push_back(card);
}

void Player::removeTraitorCard(const std::string& card) {
	auto it = std::find(traitorCards.begin(), traitorCards.end(), card);
	if (it != traitorCards.end()) {
		traitorCards.erase(it);
	}
}

int Player::getFactionIndex() const { 
	return factionIndex; 
}

const std::string& Player::getFactionName() const { 
	return factionName; 
}

int Player::getHomeSector() const { 
	return homeSectorIndex; 
}

int Player::getSpice() const { 
	return spice; 
}

int Player::getUnitsReserve() const { 
	return reserveUnits.normal; 
}

int Player::getEliteUnitsReserve() const { 
	return reserveUnits.elite; 
}

int Player::getUnitsDeployed() const { 
	return deployedUnits.normal; 
}

int Player::getEliteUnitsDeployed() const { 
	return deployedUnits.elite; 
}

int Player::getUnitsDestroyed() const { 
	return destroyedUnits.normal; 
}

int Player::getEliteUnitsDestroyed() const { 
	return destroyedUnits.elite; 
}

int Player::getTotalUnits() const { 
	return reserveUnits.total() + deployedUnits.total(); 
}

int Player::getFreeRevivesPerTurn() const {
	return std::max(0, freeReviveModifier);
}

const std::vector<std::string>& Player::getTreacheryCards() const { 
	return treacheryCards; 
}

const std::vector<std::string>& Player::getTraitorCards() const { 
	return traitorCards; 
}

void Player::addLeader(const Leader& leader) {
	aliveLeaders.push_back(leader);
}

void Player::killLeader(size_t aliveIndex) {
	if (aliveIndex < aliveLeaders.size()) {
		deadLeaders.push_back(aliveLeaders[aliveIndex]);
		aliveLeaders.erase(aliveLeaders.begin() + aliveIndex);
	}
}

void Player::reviveLeader(size_t deadIndex) {
	if (deadIndex < deadLeaders.size()) {
		aliveLeaders.push_back(deadLeaders[deadIndex]);
		deadLeaders.erase(deadLeaders.begin() + deadIndex);
	}
}

const std::vector<Leader>& Player::getAliveLeaders() const {
	return aliveLeaders;
}

std::vector<Leader>& Player::getAliveLeadersMutable() {
	return aliveLeaders;
}

void Player::markLeaderBattled(size_t aliveLeaderIndex) {
	if (aliveLeaderIndex < aliveLeaders.size()) {
		aliveLeaders[aliveLeaderIndex].hasBattled = true;
	}
}

const std::vector<Leader>& Player::getDeadLeaders() const {
	return deadLeaders;
}

void Player::resetLeaderBattleStatus() {
	for (auto& leader : aliveLeaders) {
		leader.hasBattled = false;
	}
}
