/*
===========================================================================
Quake 3 UI Module
===========================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "ui_public.h"

// Function prototypes
Q_EXPORT void QDECL dllEntry_ui( dllSyscall_t syscallptr );
Q_EXPORT intptr_t QDECL vmMain_ui( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2 );

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
Q_EXPORT intptr_t QDECL vmMain_ui( int command, intptr_t arg0, intptr_t arg1 __attribute__((unused)), intptr_t arg2 __attribute__((unused)) ) {
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
            // Draw a simple background to confirm UI rendering works
            static qhandle_t uiWhiteShader = 0;
            if (uiWhiteShader == 0) {
                uiWhiteShader = syscall( UI_R_REGISTERSHADERNOMIP, "white" );
                if (!uiWhiteShader) {
                    uiWhiteShader = syscall( UI_R_REGISTERSHADERNOMIP, "gfx/2d/bigchars" );
                }
            }

            vec4_t color = {0.2f, 0.2f, 0.2f, 1.0f};
            int color_int;
            memcpy(&color_int, &color, sizeof(int)); // Avoid strict aliasing violation
            syscall( UI_R_SETCOLOR, color_int );
            syscall( UI_R_DRAWSTRETCHPIC, 0, 0, 640, 480, 0, 0, 1, 1, uiWhiteShader );
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