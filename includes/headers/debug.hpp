#pragma once

#include <string>

// Forward declaration
class Game;

class GameDebugger {
	private:
		static Game* gameInstance;

	public:
		static void setGameInstance(Game* game);
		static void printGameState();
};
