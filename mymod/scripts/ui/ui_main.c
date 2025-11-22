/*
===========================================================================
UI Module - Native C Implementation
===========================================================================

This is an example UI module compiled as a native shared library.
You can use modern C features (C11+) instead of QVM's limited C compiler.

Required exports:
- dllEntry() - Called when module is loaded
- vmMain() - Main entry point for UI logic

===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "ui_public.h"

// System call function pointer (set by dllEntry)
static dllSyscall_t syscallPtr = NULL;

/*
=================
dllEntry

Called when the module is loaded. Sets up the system call interface.
=================
*/
__attribute__((visibility("default"))) void dllEntry( dllSyscall_t syscallptr ) {
	syscallPtr = syscallptr;
}

/*
=================
vmMain

Main entry point for UI module. Called by the engine for various commands.
=================
*/
__attribute__((visibility("default"))) intptr_t vmMain( int command, int arg0, int arg1, int arg2 ) {
	(void)arg0; (void)arg1; (void)arg2; // Suppress unused parameter warnings
	switch( command ) {
		case UI_GETAPIVERSION:
			// Return API version
			return UI_API_VERSION;
			
		case UI_INIT:
			// Initialize UI module
			// arg0 = clTime
			// arg1 = cls.state
			return 0;
			
		case UI_SHUTDOWN:
			// Shutdown UI module
			return 0;
			
		case UI_KEY_EVENT:
			// Key event
			// arg0 = key
			// arg1 = down
			return 0;
			
		case UI_MOUSE_EVENT:
			// Mouse event
			// arg0 = dx
			// arg1 = dy
			return 0;
			
		case UI_REFRESH:
			// Refresh UI
			// arg0 = realtime
			return 0;
			
		case UI_IS_FULLSCREEN:
			// Check if fullscreen
			return 0;
			
		case UI_SET_ACTIVE_MENU:
			// Set active menu
			// arg0 = menu
			return 0;
			
		case UI_CONSOLE_COMMAND:
			// Console command
			// arg0 = cmd
			return 0;
			
		case UI_DRAW_CONNECT_SCREEN:
			// Draw connect screen
			// arg0 = overlay
			return 0;
			
		default:
			return 0;
	}
}

/*
===========================================================================
Example: Using modern C features for UI code

You can use modern C features for:
- Better string handling
- Standard library data structures
- Cleaner code with C11 features
- Better performance

Example:
	void render_menu(void) {
		#include <string.h>
		#include <stdio.h>
		
		// Modern string handling
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Menu Item %d", 42);
		
		// C11 features
		for (int i = 0; i < 10; i++) {
			// Process menu items
		}
	}
===========================================================================
*/

