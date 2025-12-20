/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "ui_local.h"

void UI_SPArena_Start( const char *arenaInfo ) {
	const char	*map;
	int		level;
	int		n;
	const char	*txt;

	trap_Print( va( "DEBUG: UI_SPArena_Start called with arenaInfo=%s\n", arenaInfo ? arenaInfo : "<NULL>" ) );

	n = (int)trap_Cvar_VariableValue( "sv_maxclients" );
	if ( n < 8 ) {
		trap_Cvar_SetValue( "sv_maxclients", 8 );
	}

	level = atoi( Info_ValueForKey( arenaInfo, "num" ) );
	txt = Info_ValueForKey( arenaInfo, "special" );
	if( txt[0] ) {
		if( Q_strequal( txt, "training" ) ) {
			level = -4;
		}
		else if( Q_strequal( txt, "final" ) ) {
			level = UI_GetNumSPTiers() * ARENAS_PER_TIER;
		}
	}
	trap_Cvar_SetValue( "ui_spSelection", level );

	map = Info_ValueForKey( arenaInfo, "map" );
	trap_Print( va( "DEBUG: UI_SPArena_Start executing spmap command for map=%s\n", map ? map : "<NULL>" ) );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "spmap %s\n", map ) );
	trap_Print( "DEBUG: UI_SPArena_Start spmap command executed\n" );
}
