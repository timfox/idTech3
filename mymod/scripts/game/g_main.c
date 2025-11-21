/*
===========================================================================
Game Module - Native C Implementation
===========================================================================

This is an example game module compiled as a native shared library.
You can use modern C features (C11+) instead of QVM's limited C compiler.

Required exports:
- dllEntry() - Called when module is loaded
- vmMain() - Main entry point for game logic

===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../game/g_public.h"

// System call function pointer (set by dllEntry)
static dllSyscall_t syscallPtr = NULL;

/*
=================
dllEntry

Called when the module is loaded. Sets up the system call interface.
=================
*/
void dllEntry( dllSyscall_t syscallptr ) {
	syscallPtr = syscallptr;
}

/*
=================
vmMain

Main entry point for game module. Called by the engine for various commands.
=================
*/
intptr_t vmMain( int command, int arg0, int arg1, int arg2 ) {
	switch( command ) {
		case GAME_INIT:
			// Initialize game module
			// arg0 = levelTime
			// arg1 = randomSeed
			// arg2 = restart
			return 0;
			
		case GAME_SHUTDOWN:
			// Shutdown game module
			// arg0 = restart
			return 0;
			
		case GAME_CLIENT_CONNECT:
			// Client connecting
			// arg0 = clientNum
			// arg1 = firstTime
			// arg2 = isBot
			return 0;
			
		case GAME_CLIENT_DISCONNECT:
			// Client disconnecting
			// arg0 = clientNum
			return 0;
			
		case GAME_CLIENT_USERINFO_CHANGED:
			// Client userinfo changed
			// arg0 = clientNum
			return 0;
			
		case GAME_CLIENT_COMMAND:
			// Client command
			// arg0 = clientNum
			return 0;
			
		case GAME_CLIENT_THINK:
			// Client think
			// arg0 = clientNum
			return 0;
			
		case GAME_RUN_FRAME:
			// Run game frame
			// arg0 = levelTime
			return 0;
			
		case GAME_CONSOLE_COMMAND:
			// Console command
			return 0;
			
		default:
			return 0;
	}
}

/*
===========================================================================
Example: Using modern C features

You can now use:
- C11 features (variable-length arrays, _Generic, etc.)
- Standard library functions (malloc, free, etc.)
- Inline functions
- Function pointers
- Struct initialization
- And much more!

Example:
	void modern_function(void) {
		// C11 variable-length array
		int n = 10;
		int arr[n];
		
		// Standard library
		char *str = malloc(256);
		// ... use str ...
		free(str);
	}
===========================================================================
*/

