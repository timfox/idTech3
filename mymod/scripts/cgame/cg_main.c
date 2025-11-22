/*
===========================================================================
CGame Module - Native C Implementation
===========================================================================

This is an example cgame module compiled as a native shared library.
You can use modern C features (C11+) instead of QVM's limited C compiler.

Required exports:
- dllEntry() - Called when module is loaded
- vmMain() - Main entry point for client-side game logic

===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cg_public.h"

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

Main entry point for cgame module. Called by the engine for various commands.
=================
*/
intptr_t vmMain( int command, int arg0, int arg1, int arg2 ) {
	(void)arg0; (void)arg1; (void)arg2; // Suppress unused parameter warnings
	switch( command ) {
		case CG_INIT:
			// Initialize cgame module
			// arg0 = serverMessageNum
			// arg1 = serverCommandSequence
			// arg2 = clientNum
			return 0;
			
		case CG_SHUTDOWN:
			// Shutdown cgame module
			return 0;
			
		case CG_CONSOLE_COMMAND:
			// Console command
			return 0;
			
		case CG_DRAW_ACTIVE_FRAME:
			// Draw active frame
			// arg0 = serverTime
			// arg1 = stereoView
			// arg2 = demoPlayback
			return 0;
			
		case CG_CROSSHAIR_PLAYER:
			// Crosshair player
			// arg0 = entityNum
			return 0;
			
		case CG_LAST_ATTACKER:
			// Last attacker
			return 0;
			
		case CG_KEY_EVENT:
			// Key event
			// arg0 = key
			// arg1 = down
			return 0;
			
		case CG_MOUSE_EVENT:
			// Mouse event
			// arg0 = dx
			// arg1 = dy
			return 0;
			
		case CG_EVENT_HANDLING:
			// Event handling
			// arg0 = type
			return 0;
			
		default:
			return 0;
	}
}

/*
===========================================================================
Example: Using modern C features for client-side code

You can use modern C features for:
- Better performance with inline functions
- Standard library for data structures
- C11 features for cleaner code
- Better debugging with modern tools

Example:
	void render_effect(void) {
		// Use standard library
		#include <math.h>
		#include <string.h>
		
		float matrix[16];
		memset(matrix, 0, sizeof(matrix));
		
		// Modern C features
		for (int i = 0; i < 16; i++) {
			matrix[i] = (float)i;
		}
	}
===========================================================================
*/

