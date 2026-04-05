#include "dune.hpp"

int main()
{
	std::cout << "=== Dune Board Game Engine ===" << std::endl;
	std::cout << "Original Dune Board Game Simulator\n" << std::endl;
	
	// Create a game with 3 players, seeded with 42, with interactive mode enabled
	Game dune_game(3, 42, true);
	
	// Run the game
	dune_game.runGame();
	
	return 0;
}