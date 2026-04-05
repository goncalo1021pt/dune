#include <signal.h>
#include <iostream>
#include "debug.hpp"

void signal_handler(int signal, siginfo_t *info, void *context)
{
	(void)info;
	(void)context;
	if (signal == SIGINT)
	{
		std::cout << "\n\n[SIGNAL] Interrupt received. Exiting...\n";
		exit(0);
	}
	else if (signal == SIGQUIT)
	{
		GameDebugger::printGameState();
	}
	else if (signal == SIGUSR1)	
	{
		GameDebugger::printGameState();
	}
}

void set_signal()
{
	struct sigaction sa;

	sa.sa_sigaction = signal_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	if (sigemptyset(&sa.sa_mask) != 0)
		return ;
	
	if (sigaction(SIGINT, &sa, NULL) == -1)
		perror("sigaction SIGINT");
	if (sigaction(SIGQUIT, &sa, NULL) == -1)
		perror("sigaction SIGQUIT");
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		perror("sigaction SIGUSR1");
}
