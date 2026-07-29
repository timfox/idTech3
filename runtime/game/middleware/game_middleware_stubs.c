/*
===========================================================================
Game AI middleware OFF path (core profile). Call sites use #ifdef USE_GAME_AI_MIDDLEWARE;
this TU exists for a single startup log line when middleware sources are stripped.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifndef USE_GAME_AI_MIDDLEWARE

void GameMiddleware_LogDisabled( void ) {
	static qboolean logged;

	if ( logged ) {
		return;
	}
	logged = qtrue;
	Com_Printf( "Game AI middleware: OFF (Director/GOAP/Horde/BT — enable with IDTECH3_PROFILE=game)\n" );
}

#endif
