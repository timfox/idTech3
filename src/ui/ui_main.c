/*
===========================================================================
Quake 3 UI Module
===========================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "ui_public.h"

// UI syscall function pointer
static intptr_t (QDECL *syscall)( intptr_t arg, ... ) = NULL;

// UI state
static uiMenuCommand_t uiActiveMenu = UIMENU_NONE;

/*
===============
dllEntry_ui
===============
*/
Q_EXPORT void QDECL dllEntry_ui( dllSyscall_t syscallptr ) {
    syscall = syscallptr;
}

/*
===============
vmMain_ui
===============
*/
Q_EXPORT intptr_t QDECL vmMain_ui( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2 ) {
    switch ( command ) {
    case UI_INIT:
        // Initialize UI
        return 0;

    case UI_SHUTDOWN:
        // Shutdown UI
        return 0;

    case UI_KEY_EVENT:
        // Handle key events
        return 0;

    case UI_MOUSE_EVENT:
        // Handle mouse events
        return 0;

    case UI_REFRESH:
        // Refresh UI - draw a simple main menu
        if (uiActiveMenu == UIMENU_MAIN) {
            // Draw a simple white background
            vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
            syscall( UI_R_SETCOLOR, *(int*)&color );
            syscall( UI_R_DRAWSTRETCHPIC, 100, 100, 400, 300, 0, 0, 1, 1, 0 );
        }
        return 0;

    case UI_IS_FULLSCREEN:
        return 0; // Not fullscreen

    case UI_SET_ACTIVE_MENU:
        uiActiveMenu = (uiMenuCommand_t)arg0;
        return 0;

    case UI_CONSOLE_COMMAND:
        return 0; // No console commands handled

    case UI_DRAW_CONNECT_SCREEN:
        return 0;

    case UI_HASUNIQUECDKEY:
        return 0; // No unique CD key

    default:
        return 0;
    }
}