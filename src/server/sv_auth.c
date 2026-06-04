/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Account token verification hook for game mods (live ops v1 stub).
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "server.h"

static cvar_t *sv_authEnable;

void SV_Auth_Init( void ) {
	sv_authEnable = Cvar_Get( "sv_authEnable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_authEnable,
		"Enable SV_AuthVerifyToken stub for userinfo account tokens (game mod validates)." );
	if ( sv_authEnable->integer ) {
		Com_Printf( "[auth] sv_authEnable=1 (token hook active)\n" );
	}
}

qboolean SV_AuthVerifyToken( const char *token, int clientNum ) {
	(void)clientNum;

	if ( !sv_authEnable || !sv_authEnable->integer ) {
		return qtrue;
	}
	if ( !token || !token[0] ) {
		return qfalse;
	}
	/* v1: non-empty token passes; replace with idtech3.com REST in game mod or Phase G service. */
	return qtrue;
}
