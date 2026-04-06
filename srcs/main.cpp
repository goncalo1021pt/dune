#include "dune.hpp"

extern void set_signal();

void parsing(int argc, char* argv[], bool& debugMode) {
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "-d") {
			debugMode = true;
			std::cout << "[DEBUG MODE] Interactive terminal input enabled" << std::endl;
			break;
		}
	}
}

int main(int argc, char* argv[])
{	
	bool debugMode = false;
	parsing(argc, argv, debugMode);
	set_signal();
	
	Game dune_game(3, 42, debugMode);
	
	GameDebugger::setGameInstance(&dune_game);
	
	dune_game.runGame();
	
	return 0;
}