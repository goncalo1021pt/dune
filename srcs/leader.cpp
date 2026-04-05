#include "leader.hpp"
#include <string>

Leader::Leader(const std::string& leaderName, int p)
	: name(leaderName), power(p), hasBattled(false) {
	if (power < 1) power = 1;
	if (power > 5) power = 5;
}

Leader Leader::createDefault(int level) {
	if (level < 1) level = 1;
	if (level > 5) level = 5;
	std::string name = "default_leader_" + std::to_string(level);
	return Leader(name, level);
}
