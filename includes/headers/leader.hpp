#pragma once

#include <string>

struct Leader {
	std::string name;
	int power;          // 1-5
	bool hasBattled;    // true if leader has already battled this turn

	Leader(const std::string& leaderName = "default_leader_1", int p = 1);

	// Static factory for default leaders
	static Leader createDefault(int level);
};

