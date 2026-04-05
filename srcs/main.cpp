#include "dune.hpp"

extern void set_signal();

int main()
{
	std::cout << " === Dune Board Game Engine ===" << std::endl;
	std::cout << "Original Dune Board Game Simulator\n" << std::endl;
	
	// Set up signal handlers
	set_signal();
	
	// Create a game with 3 players, seeded with 42, with interactive mode enabled
	Game dune_game(3, 42, true);
	
	// Register game instance for debug output
	GameDebugger::setGameInstance(&dune_game);
	
	// Run the game
	dune_game.runGame();
	
	return 0;
}