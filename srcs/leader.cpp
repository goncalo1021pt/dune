#include "leader.hpp"
#include <string>

Leader::Leader(const std::string& leaderName, int p)
	: name(leaderName), power(p), hasBattled(false) {
	if (power < 1) power = 1;
	if (power > 10) power = 10;
}

Leader Leader::createDefault(int level) {
	if (level < 1) level = 1;
	if (level > 10) level = 10;
	std::string name = "default_leader_" + std::to_string(level);
	return Leader(name, level);
}

Leader Leader::createForFaction(const std::string& factionName, int level) {
	if (level < 1) level = 1;
	if (level > 10) level = 10;
	std::string name = factionName + "_" + std::to_string(level);
	return Leader(name, level);
}
