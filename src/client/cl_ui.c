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

#include "client.h"
#include "cl_steamdeck.h"
#define TRAP_EXTENSIONS_LIST ui_extensionTraps
#include "../common/vm_ext.h"
#include "../common/syscall_registry.h"

#include "../botlib/botlib.h"

extern	botlib_export_t	*botlib_export;
extern qboolean re_initialized;

static char cl_queued_intro_video[MAX_OSPATH];
static qboolean ui_initialized = qfalse;

void CL_QueueIntroVideo(const char *videoFile) {
	if (!videoFile || !videoFile[0]) {
		return;
	}
	Q_strncpyz(cl_queued_intro_video, videoFile, sizeof(cl_queued_intro_video));
}

void CL_PlayQueuedIntroVideo(void) {
	if (!re_initialized || !cl_queued_intro_video[0]) {
		return;
	}
	Com_Printf( "Playing queued intro video: %s\n", cl_queued_intro_video );
	Cbuf_AddText( va( "cinematic %s 2\n", cl_queued_intro_video ) );
	cl_queued_intro_video[0] = '\0';
}

static ext_trap_keys_t ui_extensionTraps[] = {
	{ "trap_R_AddRefEntityToScene2",       UI_R_ADDREFENTITYTOSCENE2, qfalse },
	{ "trap_R_AddLinearLightToScene_Q3E", UI_R_ADDLINEARLIGHTTOSCENE, qfalse },
	{ "trap_Cvar_SetDescription_Q3E",     UI_CVAR_SETDESCRIPTION,    qfalse },
	{ NULL,                               -1,                        qfalse }
};

vm_t *uivm = NULL;

/*
====================
GetClientState
====================
*/
static void GetClientState( uiClientState_t *state ) {
	state->connectPacketCount = clc.connectPacketCount;
	state->connState = cls.state;
	Q_strncpyz( state->servername, cls.servername, sizeof( state->servername ) );
	Q_strncpyz( state->updateInfoString, cls.updateInfoString, sizeof( state->updateInfoString ) );
	Q_strncpyz( state->messageString, clc.serverMessage, sizeof( state->messageString ) );
	state->clientNum = cl.snap.ps.clientNum;
}


/*
====================
LAN_LoadCachedServers
====================
*/
static void LAN_LoadCachedServers( void ) {
	fileHandle_t fileIn;
	int size, file_size;

	cls.numglobalservers = cls.numfavoriteservers = 0;
	cls.numGlobalServerAddresses = 0;

	file_size = FS_Home_FOpenFileRead( "servercache.dat", &fileIn );
	if ( file_size < (int)(3*sizeof(int)) ) {
		if ( fileIn != FS_INVALID_HANDLE ) {
			FS_FCloseFile( fileIn );
		}
		return;
	}

	FS_Read( &cls.numglobalservers, sizeof(int), fileIn );
	FS_Read( &cls.numfavoriteservers, sizeof(int), fileIn );
	FS_Read( &size, sizeof(int), fileIn );

	if ( size == sizeof(cls.globalServers) + sizeof(cls.favoriteServers) ) {
		FS_Read( &cls.globalServers, sizeof(cls.globalServers), fileIn );
		FS_Read( &cls.favoriteServers, sizeof(cls.favoriteServers), fileIn );
	} else {
		cls.numglobalservers = cls.numfavoriteservers = 0;
		cls.numGlobalServerAddresses = 0;
	}

	FS_FCloseFile( fileIn );
}


/*
====================
LAN_SaveServersToCache
====================
*/
static void LAN_SaveServersToCache( void ) {
	fileHandle_t fileOut;
	int size;

	fileOut = FS_FOpenFileWrite( "servercache.dat" );
	if ( fileOut == FS_INVALID_HANDLE )
		return;

	FS_Write(&cls.numglobalservers, sizeof(int), fileOut);
	FS_Write(&cls.numfavoriteservers, sizeof(int), fileOut);
	size = sizeof(cls.globalServers) + sizeof(cls.favoriteServers);
	FS_Write(&size, sizeof(int), fileOut);
	FS_Write(&cls.globalServers, sizeof(cls.globalServers), fileOut);
	FS_Write(&cls.favoriteServers, sizeof(cls.favoriteServers), fileOut);

	FS_FCloseFile(fileOut);
}


/*
====================
LAN_ResetPings
====================
*/
static void LAN_ResetPings(int source) {
	int count,i;
	serverInfo_t *servers = NULL;
	count = 0;

	switch (source) {
		case AS_LOCAL :
			servers = &cls.localServers[0];
			count = MAX_OTHER_SERVERS;
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			servers = &cls.globalServers[0];
			count = MAX_GLOBAL_SERVERS;
			break;
		case AS_FAVORITES :
			servers = &cls.favoriteServers[0];
			count = MAX_OTHER_SERVERS;
			break;
	}
	if (servers) {
		for (i = 0; i < count; i++) {
			servers[i].ping = -1;
		}
	}
}

void CL_EnsureUIInitialized(void) {
	if (ui_initialized || !uivm) {
		return;
	}
	VM_Call(uivm, 1, UI_INIT, cls.realtime);
	ui_initialized = qtrue;
}


/*
====================
LAN_AddServer
====================
*/
static int LAN_AddServer(int source, const char *name, const char *address) {
	int max, *count, i;
	netadr_t adr;
	serverInfo_t *servers = NULL;
	max = MAX_OTHER_SERVERS;
	count = NULL;

	switch (source) {
		case AS_LOCAL :
			count = &cls.numlocalservers;
			servers = &cls.localServers[0];
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			max = MAX_GLOBAL_SERVERS;
			count = &cls.numglobalservers;
			servers = &cls.globalServers[0];
			break;
		case AS_FAVORITES :
			count = &cls.numfavoriteservers;
			servers = &cls.favoriteServers[0];
			break;
	}
	if (servers && *count < max) {
		NET_StringToAdr( address, &adr, NA_UNSPEC );
		for ( i = 0; i < *count; i++ ) {
			if (NET_CompareAdr(&servers[i].adr, &adr)) {
				break;
			}
		}
		if (i >= *count) {
			servers[*count].adr = adr;
			Q_strncpyz(servers[*count].hostName, name, sizeof(servers[*count].hostName));
			servers[*count].visible = qtrue;
			(*count)++;
			return 1;
		}
		return 0;
	}
	return -1;
}


/*
====================
LAN_RemoveServer
====================
*/
static void LAN_RemoveServer(int source, const char *addr) {
	int *count, i;
	serverInfo_t *servers = NULL;
	count = NULL;
	switch (source) {
		case AS_LOCAL :
			count = &cls.numlocalservers;
			servers = &cls.localServers[0];
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			count = &cls.numglobalservers;
			servers = &cls.globalServers[0];
			break;
		case AS_FAVORITES :
			count = &cls.numfavoriteservers;
			servers = &cls.favoriteServers[0];
			break;
	}
	if (servers) {
		netadr_t comp;
		NET_StringToAdr( addr, &comp, NA_UNSPEC );
		for (i = 0; i < *count; i++) {
			if (NET_CompareAdr( &comp, &servers[i].adr)) {
				int j = i;
				while (j < *count - 1) {
					Com_Memcpy(&servers[j], &servers[j+1], sizeof(servers[j]));
					j++;
				}
				(*count)--;
				break;
			}
		}
	}
}


/*
====================
LAN_GetServerCount
====================
*/
static int LAN_GetServerCount( int source ) {
	switch (source) {
		case AS_LOCAL :
			return cls.numlocalservers;
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			return cls.numglobalservers;
			break;
		case AS_FAVORITES :
			return cls.numfavoriteservers;
			break;
	}
	return 0;
}


/*
====================
LAN_GetLocalServerAddressString
====================
*/
static void LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.localServers[n].adr) , buflen );
				return;
			}
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.globalServers[n].adr) , buflen );
				return;
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				Q_strncpyz(buf, NET_AdrToStringwPort( &cls.favoriteServers[n].adr) , buflen );
				return;
			}
			break;
	}
	buf[0] = '\0';
}


/*
====================
LAN_GetServerInfo
====================
*/
static void LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	char info[MAX_STRING_CHARS];
	serverInfo_t *server = NULL;
	info[0] = '\0';
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.localServers[n];
			}
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				server = &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.favoriteServers[n];
			}
			break;
	}
	if (server && buf && buflen > 0) {
		buf[0] = '\0';
		Info_SetValueForKey( info, "hostname", server->hostName);
		Info_SetValueForKey( info, "mapname", server->mapName);
		Info_SetValueForKey( info, "clients", va("%i",server->clients));
		Info_SetValueForKey( info, "sv_maxclients", va("%i",server->maxClients));
		Info_SetValueForKey( info, "ping", va("%i",server->ping));
		Info_SetValueForKey( info, "minping", va("%i",server->minPing));
		Info_SetValueForKey( info, "maxping", va("%i",server->maxPing));
		Info_SetValueForKey( info, "game", server->game);
		Info_SetValueForKey( info, "gametype", va("%i",server->gameType));
		Info_SetValueForKey( info, "nettype", va("%i",server->netType));
		Info_SetValueForKey( info, "addr", NET_AdrToStringwPort(&server->adr));
		Info_SetValueForKey( info, "punkbuster", va("%i", server->punkbuster));
		Info_SetValueForKey( info, "g_needpass", va("%i", server->g_needpass));
		Info_SetValueForKey( info, "g_humanplayers", va("%i", server->g_humanplayers));
		Q_strncpyz(buf, info, buflen);
	} else {
		if (buf && buflen > 0) {
			buf[0] = '\0';
		}
	}
}


/*
====================
LAN_GetServerPing
====================
*/
static int LAN_GetServerPing( int source, int n ) {
	serverInfo_t *server = NULL;
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.localServers[n];
			}
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				server = &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				server = &cls.favoriteServers[n];
			}
			break;
	}
	if (server) {
		return server->ping;
	}
	return -1;
}

/*
====================
LAN_GetServerPtr
====================
*/
static serverInfo_t *LAN_GetServerPtr( int source, int n ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return &cls.localServers[n];
			}
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				return &cls.globalServers[n];
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return &cls.favoriteServers[n];
			}
			break;
	}
	return NULL;
}


/*
====================
LAN_CompareServers
====================
*/
static int LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	int res;
	serverInfo_t *server1, *server2;

	server1 = LAN_GetServerPtr(source, s1);
	server2 = LAN_GetServerPtr(source, s2);
	if (!server1 || !server2) {
		return 0;
	}

	res = 0;
	switch( sortKey ) {
		case SORT_HOST:
			res = Q_stricmp( server1->hostName, server2->hostName );
			break;

		case SORT_MAP:
			res = Q_stricmp( server1->mapName, server2->mapName );
			break;
		case SORT_CLIENTS:
			if (server1->clients < server2->clients) {
				res = -1;
			}
			else if (server1->clients > server2->clients) {
				res = 1;
			}
			else {
				res = 0;
			}
			break;
		case SORT_GAME:
			if (server1->gameType < server2->gameType) {
				res = -1;
			}
			else if (server1->gameType > server2->gameType) {
				res = 1;
			}
			else {
				res = 0;
			}
			break;
		case SORT_PING:
			if (server1->ping < server2->ping) {
				res = -1;
			}
			else if (server1->ping > server2->ping) {
				res = 1;
			}
			else {
				res = 0;
			}
			break;
	}

	if (sortDir) {
		if (res < 0)
			return 1;
		if (res > 0)
			return -1;
		return 0;
	}
	return res;
}


/*
====================
LAN_GetPingQueueCount
====================
*/
static int LAN_GetPingQueueCount( void ) {
	return (CL_GetPingQueueCount());
}


/*
====================
LAN_ClearPing
====================
*/
static void LAN_ClearPing( int n ) {
	CL_ClearPing( n );
}


/*
====================
LAN_GetPing
====================
*/
static void LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	CL_GetPing( n, buf, buflen, pingtime );
}


/*
====================
LAN_GetPingInfo
====================
*/
static void LAN_GetPingInfo( int n, char *buf, int buflen ) {
	CL_GetPingInfo( n, buf, buflen );
}


/*
====================
LAN_MarkServerVisible
====================
*/
static void LAN_MarkServerVisible(int source, int n, qboolean visible ) {
	if (n == -1) {
		int count = MAX_OTHER_SERVERS;
		serverInfo_t *server = NULL;
		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				break;
			case AS_MPLAYER:
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				count = MAX_GLOBAL_SERVERS;
				break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				break;
		}
		if (server) {
			for (n = 0; n < count; n++) {
				server[n].visible = visible;
			}
		}

	} else {
		switch (source) {
			case AS_LOCAL :
				if (n >= 0 && n < MAX_OTHER_SERVERS) {
					cls.localServers[n].visible = visible;
				}
				break;
			case AS_MPLAYER:
			case AS_GLOBAL :
				if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
					cls.globalServers[n].visible = visible;
				}
				break;
			case AS_FAVORITES :
				if (n >= 0 && n < MAX_OTHER_SERVERS) {
					cls.favoriteServers[n].visible = visible;
				}
				break;
		}
	}
}


/*
=======================
LAN_ServerIsVisible
=======================
*/
static int LAN_ServerIsVisible(int source, int n ) {
	switch (source) {
		case AS_LOCAL :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return cls.localServers[n].visible;
			}
			break;
		case AS_MPLAYER:
		case AS_GLOBAL :
			if (n >= 0 && n < MAX_GLOBAL_SERVERS) {
				return cls.globalServers[n].visible;
			}
			break;
		case AS_FAVORITES :
			if (n >= 0 && n < MAX_OTHER_SERVERS) {
				return cls.favoriteServers[n].visible;
			}
			break;
	}
	return qfalse;
}


/*
=======================
LAN_UpdateVisiblePings
=======================
*/
static qboolean LAN_UpdateVisiblePings(int source ) {
	return CL_UpdateVisiblePings_f(source);
}


/*
====================
LAN_GetServerStatus
====================
*/
static int LAN_GetServerStatus( const char *serverAddress, char *serverStatus, int maxLen ) {
	return CL_ServerStatus( serverAddress, serverStatus, maxLen );
}


/*
====================
CL_GetGlConfig
====================
*/
static void CL_GetGlconfig( glconfig_t *config ) {
	*config = *re.GetConfig();
}


/*
====================
CL_GetClipboardData
====================
*/
static void CL_GetClipboardData( char *buf, int buflen ) {
	char	*cbd;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		*buf = '\0';
		return;
	}

	Q_strncpyz( buf, cbd, buflen );

	Z_Free( cbd );
}


/*
====================
Key_KeynumToStringBuf
====================
*/
static void Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) {
	Q_strncpyz( buf, Key_KeynumToString( keynum ), buflen );
}


/*
====================
Key_GetBindingBuf
====================
*/
static void Key_GetBindingBuf( int keynum, char *buf, int buflen ) {
	const char *value;

	value = Key_GetBinding( keynum );
	if ( value ) {
		Q_strncpyz( buf, value, buflen );
	}
	else {
		*buf = '\0';
	}
}


/*
====================
CLUI_GetCDKey
====================
*/
static void CLUI_GetCDKey( char *buf, int buflen ) {
#ifndef STANDALONE
	// CD key screen disabled - always return a valid-looking default CD key
	// This prevents the UI from showing the CD key screen
	Q_strncpyz( buf, "1234567890123456", buflen );
	buf[16] = '\0';
	// Original code commented out:
	// const char *gamedir;
	// gamedir = Cvar_VariableString( "fs_game" );
	// if ( UI_usesUniqueCDKey() && gamedir[0] != '\0' ) {
	// 	Com_Memcpy( buf, &cl_cdkey[16], 16 );
	// 	buf[16] = '\0';
	// } else {
	// 	Com_Memcpy( buf, cl_cdkey, 16 );
	// 	buf[16] = '\0';
	// }
#else
	*buf = '\0';
#endif
}


/*
====================
CLUI_SetCDKey
====================
*/
#ifndef STANDALONE
static void CLUI_SetCDKey( char *buf ) {
	const char *gamedir;
	gamedir = Cvar_VariableString( "fs_game" );
	if ( qfalse && gamedir[0] != '\0' ) {
		Com_Memcpy( &cl_cdkey[16], buf, 16 );
		cl_cdkey[32] = '\0';
		// set the flag so the flag will be written at the next opportunity
		cvar_modifiedFlags |=(CVAR_ARCHIVE);
	} else {
		Com_Memcpy( cl_cdkey, buf, 16 );
		// set the flag so the flag will be written at the next opportunity
		cvar_modifiedFlags |=(CVAR_ARCHIVE);
	}
}
#endif


/*
====================
GetConfigString
====================
*/
static int GetConfigString(int index, char *buf, int size)
{
	int		offset;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		return qfalse;

	offset = cl.gameState.stringOffsets[index];
	if (!offset) {
		if( size ) {
			buf[0] = 0;
		}
		return qfalse;
	}

	Q_strncpyz( buf, cl.gameState.stringData+offset, size);

	return qtrue;
}


/*
====================
FloatAsInt
====================
*/
static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || uivm == NULL )
	  return NULL;

	if ( uivm->entryPoint )
		return (void *)(intValue);
	else
		return (void *)(uivm->dataBase + (intValue & uivm->dataMask));
}


static qboolean UI_GetValue( char* value, int valueSize, const char* key ) {
	// First, try the extension table so we can track active extensions
	if ( VM_Ext_GetKey( value, valueSize, key ) ) {
		return qtrue;
	}

	// Use centralized syscall registry for extension lookups
	// All UI extensions are now registered in syscall_registry.c
	if ( Syscall_GetValue( VM_UI, value, valueSize, key ) ) {
		return qtrue;
	}

	return qfalse;
}


/*
====================
CL_UISystemCalls

The ui module is making a system call
====================
*/
static intptr_t CL_UISystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case UI_ERROR:
	{
		const char *msg = (const char*)VMA(1);
		// Avoid blowing up on malformed UI errors
		if ( !msg || !msg[0] ) {
			Com_Printf( "UI_ERROR: empty message (ignored)\n" );
			return 0;
		}
		Com_Error( ERR_DROP, "%s", msg );
		return 0;
	}

	case UI_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;

	case UI_MILLISECONDS:
		return Sys_Milliseconds();

	case UI_CVAR_REGISTER:
	{
		const char *name = VMA(2);
		if ( !name || !name[0] ) {
			Com_Printf( "UI_CVAR_REGISTER: NULL/empty var_name (ignored)\n" );
			return 0;
		}
		Cvar_Register( VMA(1), name, VMA(3), args[4], uivm->privateFlag );
		return 0;
	}

	case UI_CVAR_UPDATE:
	{
		vmCvar_t *cvar = VMA(1);
		if ( !cvar ) {
			Com_Printf( "UI_CVAR_UPDATE: NULL vmCvar (ignored)\n" );
			return 0;
		}
		Cvar_Update( cvar, uivm->privateFlag );
		return 0;
	}

	case UI_CVAR_SET:
	{
		const char *name = VMA(1);
		const char *val  = VMA(2);
		static int emptySetWarnings = 0;
		if (!name || !name[0]) {
			if (emptySetWarnings < 5) {
				Com_Printf( "UI_CVAR_SET: NULL/empty var_name (ignored)\n" );
			} else if (emptySetWarnings == 5) {
				Com_Printf( "UI_CVAR_SET: further NULL/empty var_name warnings suppressed\n" );
			}
			++emptySetWarnings;
			return 0;
		}
		if (!val) {
			Com_Printf( "UI_CVAR_SET: NULL value (ignored)\n" );
			return 0;
		}
		Cvar_SetSafe( name, val );
		return 0;
	}

	case UI_CVAR_VARIABLEVALUE:
	{
		const char *name = VMA(1);
		if (!name || !name[0]) {
			Com_Printf( "UI_CVAR_VARIABLEVALUE: NULL/empty var_name (ignored)\n" );
			return 0;
		}
		return FloatAsInt( Cvar_VariableValue( name ) );
	}

	case UI_CVAR_VARIABLESTRINGBUFFER:
	{
		const char *name = VMA(1);
		char *buffer   = VMA(2);
		int   bufsize  = args[3];

		if (!name || !name[0]) {
			Com_Printf( "UI_CVAR_VARIABLESTRINGBUFFER: NULL/empty var_name (ignored)\n" );
			return 0;
		}

		// Defensive guard: ignore obviously bad pointers/sizes to avoid crashing in Q_strncpyz
		if ( !buffer || bufsize <= 0 || bufsize > 8192 ) {
			Com_Printf( "UI_CVAR_VARIABLESTRINGBUFFER: invalid buffer (%p) or size (%d), ignoring\n", buffer, bufsize );
			if ( buffer && bufsize > 0 ) {
				buffer[0] = '\0';
			}
			return 0;
		}

		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( name, buffer, bufsize, CVAR_PRIVATE );
		return 0;
	}

	case UI_CVAR_SETVALUE:
	{
		const char *name = VMA(1);
		if (!name || !name[0]) {
			Com_Printf( "UI_CVAR_SETVALUE: NULL/empty var_name (ignored)\n" );
			return 0;
		}
		Cvar_SetValueSafe( name, VMF(2) );
		return 0;
	}

	case UI_CVAR_RESET:
	{
		const char *name = VMA(1);
		if (!name || !name[0]) {
			Com_Printf( "UI_CVAR_RESET: NULL/empty var_name (ignored)\n" );
			return 0;
		}
		Cvar_Reset( name );
		return 0;
	}

	case UI_CVAR_CREATE:
	{
		const char *name = VMA(1);
		if (!name || !name[0]) {
			Com_Printf( "UI_CVAR_CREATE: NULL/empty var_name (ignored)\n" );
			return 0;
		}
		Cvar_Register( NULL, name, VMA(2), args[3], uivm->privateFlag );
		return 0;
	}

	case UI_CVAR_INFOSTRINGBUFFER:
	{
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Cvar_InfoStringBuffer( args[1], VMA(2), args[3] );
		return 0;
	}

	case UI_ARGC:
		return Cmd_Argc();

	case UI_ARGV:
	{
		int index = args[1];
		char *buffer = VMA(2);
		int bufsize = args[3];
		if ( !buffer || bufsize <= 0 ) {
			Com_Printf( "UI_ARGV: invalid buffer (%p) or size (%d)\n", buffer, bufsize );
			return 0;
		}
		if ( index < 0 || index >= Cmd_Argc() ) {
			buffer[0] = '\0';
			return 0;
		}
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Cmd_ArgvBuffer( index, buffer, bufsize );
		return 0;
	}

	case UI_CMD_EXECUTETEXT:
	{
		const char *text = VMA(2);
		if ( !text || !text[0] ) {
			Com_Printf( "UI_CMD_EXECUTETEXT: empty command ignored\n" );
			return 0;
		}
		if(args[1] == EXEC_NOW
		&& (!strncmp(text, "snd_restart", 11)
		|| !strncmp(text, "vid_restart", 11)
		|| !strncmp(text, "disconnect", 10)
		|| !strncmp(text, "quit", 5)))
		{
			Com_Printf (S_COLOR_YELLOW "turning EXEC_NOW '%.11s' into EXEC_INSERT\n", text);
			args[1] = EXEC_INSERT;
		}
		Cbuf_ExecuteText( args[1], text );
		return 0;
	}

	case UI_FS_FOPENFILE:
	{
		const char *qpath = VMA(1);
		if ( !qpath || !qpath[0] ) {
			Com_Printf( "UI_FS_FOPENFILE: empty filename ignored\n" );
			return -1;
		}
		return FS_VM_OpenFile( qpath, VMA(2), args[3], H_Q3UI );
	}

	case UI_FS_READ:
	{
		void *buf = VMA(1);
		int len = args[2];
		if ( !buf || len <= 0 ) {
			return 0;
		}
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		FS_VM_ReadFile( buf, len, args[3], H_Q3UI );
		return 0;
	}

	case UI_FS_WRITE:
	{
		void *buf = VMA(1);
		int len = args[2];
		if ( !buf || len <= 0 ) {
			return 0;
		}
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		FS_VM_WriteFile( buf, len, args[3], H_Q3UI );
		return 0;
	}

	case UI_FS_FCLOSEFILE:
		FS_VM_CloseFile( args[1], H_Q3UI );
		return 0;

	case UI_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], args[3], H_Q3UI );

	case UI_FS_GETFILELIST:
	{
		const char *path = VMA(1);
		const char *ext  = VMA(2);
		void *list = VMA(3);
		int size = args[4];
		if ( !list || size <= 0 ) {
			return 0;
		}
		if ( !path || !path[0] ) {
			return 0;
		}
		VM_CHECKBOUNDS( uivm, args[3], args[4] );
		return FS_GetFileList( path, ext, list, size );
	}

	case UI_R_REGISTERMODEL:
		return re.RegisterModel( VMA(1) );

	case UI_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );

	case UI_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( VMA(1) );

	case UI_R_CLEARSCENE:
		re.ClearScene();
		return 0;

	case UI_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( VMA(1), qfalse );
		return 0;

	case UI_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), 1 );
		return 0;

	case UI_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;

	case UI_R_RENDERSCENE:
		re.RenderScene( VMA(1) );
		return 0;

	case UI_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;

	case UI_R_DRAWSTRETCHPIC:
		{
			static int logged_draw = 0;
			if (logged_draw < 5) {
				Com_Printf("UI_R_DRAWSTRETCHPIC[%d]: shader=%d x=%.1f y=%.1f w=%.1f h=%.1f\n",
				           logged_draw, (int)args[9], VMF(1), VMF(2), VMF(3), VMF(4));
				logged_draw++;
			}
		}
		{
			qhandle_t shader = (args[9] != 0) ? (qhandle_t)args[9] : cls.whiteShader;
			re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), shader );
		}
		return 0;

	case UI_R_MODELBOUNDS:
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;

	case UI_UPDATESCREEN:
		SCR_UpdateScreen();
		return 0;

	case UI_CM_LERPTAG:
		re.LerpTag( VMA(1), args[2], args[3], args[4], VMF(5), VMA(6) );
		return 0;

	case UI_S_REGISTERSOUND:
		return S_RegisterSound( VMA(1), args[2] );

	case UI_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;

	case UI_KEY_KEYNUMTOSTRINGBUF:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Key_KeynumToStringBuf( args[1], VMA(2), args[3] );
		return 0;

	case UI_KEY_GETBINDINGBUF:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Key_GetBindingBuf( args[1], VMA(2), args[3] );
		return 0;

	case UI_KEY_SETBINDING:
		Key_SetBinding( args[1], VMA(2) );
		return 0;

	case UI_KEY_ISDOWN:
		return Key_IsDown( args[1] );

	case UI_KEY_GETOVERSTRIKEMODE:
		return Key_GetOverstrikeMode();

	case UI_KEY_SETOVERSTRIKEMODE:
		Key_SetOverstrikeMode( args[1] );
		return 0;

	case UI_KEY_CLEARSTATES:
		Key_ClearStates();
		return 0;

	case UI_KEY_GETCATCHER:
		return Key_GetCatcher();

	case UI_KEY_SETCATCHER:
		// Don't allow the ui module to close the console
		Key_SetCatcher( args[1] | ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) );
		return 0;

	case UI_GETCLIPBOARDDATA:
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		CL_GetClipboardData( VMA(1), args[2] );
		return 0;

	case UI_GETCLIENTSTATE:
		VM_CHECKBOUNDS( uivm, args[1], sizeof( uiClientState_t ) );
		GetClientState( VMA(1) );
		return 0;

	case UI_GETGLCONFIG:
		VM_CHECKBOUNDS( uivm, args[1], sizeof( glconfig_t ) );
		CL_GetGlconfig( VMA(1) );
		return 0;

	case UI_GETCONFIGSTRING:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		return GetConfigString( args[1], VMA(2), args[3] );

	case UI_LAN_LOADCACHEDSERVERS:
		LAN_LoadCachedServers();
		return 0;

	case UI_LAN_SAVECACHEDSERVERS:
		LAN_SaveServersToCache();
		return 0;

	case UI_LAN_ADDSERVER:
		return LAN_AddServer(args[1], VMA(2), VMA(3));

	case UI_LAN_REMOVESERVER:
		LAN_RemoveServer(args[1], VMA(2));
		return 0;

	case UI_LAN_GETPINGQUEUECOUNT:
		return LAN_GetPingQueueCount();

	case UI_LAN_CLEARPING:
		LAN_ClearPing( args[1] );
		return 0;

	case UI_LAN_GETPING:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		LAN_GetPing( args[1], VMA(2), args[3], VMA(4) );
		return 0;

	case UI_LAN_GETPINGINFO:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		LAN_GetPingInfo( args[1], VMA(2), args[3] );
		return 0;

	case UI_LAN_GETSERVERCOUNT:
		return LAN_GetServerCount(args[1]);

	case UI_LAN_GETSERVERADDRESSSTRING:
		VM_CHECKBOUNDS( uivm, args[3], args[4] );
		LAN_GetServerAddressString( args[1], args[2], VMA(3), args[4] );
		return 0;

	case UI_LAN_GETSERVERINFO:
		VM_CHECKBOUNDS( uivm, args[3], args[4] );
		LAN_GetServerInfo( args[1], args[2], VMA(3), args[4] );
		return 0;

	case UI_LAN_GETSERVERPING:
		return LAN_GetServerPing( args[1], args[2] );

	case UI_LAN_MARKSERVERVISIBLE:
		LAN_MarkServerVisible( args[1], args[2], args[3] );
		return 0;

	case UI_LAN_SERVERISVISIBLE:
		return LAN_ServerIsVisible( args[1], args[2] );

	case UI_LAN_UPDATEVISIBLEPINGS:
		return LAN_UpdateVisiblePings( args[1] );

	case UI_LAN_RESETPINGS:
		LAN_ResetPings( args[1] );
		return 0;

	case UI_LAN_SERVERSTATUS:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		return LAN_GetServerStatus( VMA(1), VMA(2), args[3] );

	case UI_LAN_COMPARESERVERS:
		return LAN_CompareServers( args[1], args[2], args[3], args[4], args[5] );

	case UI_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();

	case UI_GET_CDKEY:
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		CLUI_GetCDKey( VMA(1), args[2] );
		return 0;

	case UI_SET_CDKEY:
#ifndef STANDALONE
		CLUI_SetCDKey( VMA(1) );
#endif
		return 0;

	case UI_SET_PBCLSTATUS:
		return 0;

	case UI_R_REGISTERFONT:
		re.RegisterFont( VMA(1), args[2], VMA(3));
		return 0;

	case UI_R_FONT_HEIGHT:
		return FloatAsInt( re.Font_Height( VMA(1), VMF(2) ) );

	case UI_R_FONT_WIDTH:
		return FloatAsInt( re.Font_Width( VMA(1), VMF(2), VMA(3) ) );

	case UI_R_FONT_DRAWSTRING:
		re.Font_DrawString( VMF(1), VMF(2), VMA(3), VMA(4), VMF(5), VMA(6), (int)args[7] );
		return 0;

	// shared syscalls

	case TRAP_MEMSET:
		VM_CHECKBOUNDS( uivm, args[1], args[3] );
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];

	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( uivm, args[1], args[2], args[3] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( uivm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );

	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );

	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case UI_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case UI_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );

	case UI_PC_ADD_GLOBAL_DEFINE:
		if (!botlib_export) return 0;
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case UI_PC_LOAD_SOURCE:
		if (!botlib_export) {
			Com_Printf("WARNING: botlib_export is NULL in UI_PC_LOAD_SOURCE\n");
			return 0;
		}
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case UI_PC_FREE_SOURCE:
		if (!botlib_export) return 0;
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case UI_PC_READ_TOKEN:
		if (!botlib_export) return 0;
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case UI_PC_SOURCE_FILE_AND_LINE:
		if (!botlib_export) return 0;
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case UI_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;
	case UI_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2));
		return 0;
	case UI_S_SHOWMOUSE:
		IN_ShowMouse( args[1] );
		return 0;

	case UI_REAL_TIME:
		return Com_RealTime( VMA(1) );

	case UI_CIN_PLAYCINEMATIC:
		Com_DPrintf("UI_CIN_PlayCinematic\n");
		return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

	case UI_CIN_STOPCINEMATIC:
		return CIN_StopCinematic(args[1]);

	case UI_CIN_RUNCINEMATIC:
		return CIN_RunCinematic(args[1]);

	case UI_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic(args[1]);
		return 0;

	case UI_CIN_SETEXTENTS:
		CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
		return 0;

	case UI_R_REMAP_SHADER:
		re.RemapShader( VMA(1), VMA(2), VMA(3) );
		return 0;

	case UI_VERIFY_CDKEY:
		// CD key verification disabled - always return valid
		return qtrue;
		// return Com_CDKeyValidate(VMA(1), VMA(2));

	// engine extensions
	case UI_R_ADDREFENTITYTOSCENE2:
		re.AddRefEntityToScene( VMA(1), qtrue );
		return 0;

	// engine extensions
	case UI_R_ADDLINEARLIGHTTOSCENE:
		re.AddLinearLightToScene( VMA(1), VMA(2), VMF(3), VMF(4), VMF(5), VMF(6) );
		return 0;

	case UI_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( (const char*)VMA(1), (const char*)VMA(2) );
		return 0;

	case UI_TRAP_GETVALUE:
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		return UI_GetValue( VMA(1), args[2], VMA(3) );

	// Steam Deck extensions
	case UI_STEAMDECK_SHOW_TEXTINPUT:
		return CL_SteamDeck_ShowTextInput( VMA(1), VMA(2), args[3], args[4] );
	
	case UI_STEAMDECK_SHOW_FLOATING_TEXTINPUT:
		return CL_SteamDeck_ShowFloatingTextInput( args[1], args[2], args[3], args[4] );
	
	case UI_STEAMDECK_IS_TEXTINPUT_ACTIVE:
		return CL_SteamDeck_IsTextInputActive();
	
	case UI_STEAMDECK_GET_TEXTINPUT_RESULT:
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		CL_SteamDeck_GetTextInputResult( VMA(1), args[2] );
		return 0;

	default:
		Com_Error( ERR_DROP, "Bad UI system trap: %ld", (long int) args[0] );

	}

	return 0;
}


/*
====================
UI_DllSyscall
====================
*/
#if !id386 || defined __clang__
static __attribute__((unused)) intptr_t QDECL UI_DllSyscall( intptr_t arg, ... ) {
	intptr_t	args[10]; // max.count for UI
	va_list	ap;
	size_t i;

	args[0] = arg;
	va_start( ap, arg );
	for (i = 1; i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return CL_UISystemCalls( args );
}
#else
static __attribute__((unused)) intptr_t QDECL UI_DllSyscall( intptr_t arg, ... ) {
	return CL_UISystemCalls( &arg );
}
#endif


/*
====================
CL_ShutdownUI
====================
*/
void CL_ShutdownUI( void ) {
	Com_Printf( "CL_ShutdownUI: Starting UI shutdown process\n" );

	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	cls.uiStarted = qfalse;
	if ( !uivm ) {
		Com_Printf( "CL_ShutdownUI: No UI VM to shutdown\n" );
		return;
	}

	Com_Printf( "CL_ShutdownUI: Calling UI_SHUTDOWN syscall on VM\n" );
	VM_Call( uivm, 0, UI_SHUTDOWN );

	Com_Printf( "CL_ShutdownUI: Freeing UI VM\n" );
	VM_Free( uivm );

	uivm = NULL;

	Com_Printf( "CL_ShutdownUI: Closing UI VM files\n" );
	FS_VM_CloseFiles( H_Q3UI );

	Com_Printf( "CL_ShutdownUI: UI shutdown complete\n" );
}


/*
====================
CL_BotLibPrint
====================
*/
static void CL_BotLibPrint(int type __attribute__((unused)), const char *fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}

/*
====================
CL_BotLibHunkAlloc
====================
*/
static void *CL_BotLibHunkAlloc(int size) {
	return Hunk_Alloc(size, h_low);
}

/*
====================
CL_InitBotLib
====================
*/
static void CL_InitBotLib( void ) {
	botlib_import_t	botlib_import;

	if (botlib_export) {
		// Already initialized
		return;
	}

	Com_Printf("Initializing botlib for client...\n");

	// Set up botlib import functions for client use
	botlib_import.Print = CL_BotLibPrint;
	botlib_import.Trace = NULL;  // Not needed for PC_* functions
	botlib_import.EntityTrace = NULL;
	botlib_import.PointContents = NULL;
	botlib_import.inPVS = NULL;
	botlib_import.BSPEntityData = NULL;
	botlib_import.BSPModelMinsMaxsOrigin = NULL;
	botlib_import.BotClientCommand = NULL;

	// Memory management - use client memory functions
	botlib_import.GetMemory = Z_Malloc;
	botlib_import.FreeMemory = Z_Free;
	botlib_import.AvailableMemory = Z_AvailableMemory;
	botlib_import.HunkAlloc = CL_BotLibHunkAlloc;

	// File system access
	botlib_import.FS_FOpenFile = FS_FOpenFileByMode;
	botlib_import.FS_Read = FS_Read;
	botlib_import.FS_Write = FS_Write;
	botlib_import.FS_FCloseFile = FS_FCloseFile;
	botlib_import.FS_Seek = FS_Seek;

	// Debug functions - not needed for basic PC functionality
	botlib_import.DebugLineCreate = NULL;
	botlib_import.DebugLineDelete = NULL;
	botlib_import.DebugLineShow = NULL;
	botlib_import.DebugPolygonCreate = NULL;
	botlib_import.DebugPolygonDelete = NULL;

	botlib_import.Sys_Milliseconds = Sys_Milliseconds;

	botlib_export = (botlib_export_t *)GetBotLibAPI( BOTLIB_API_VERSION, &botlib_import );
	if (!botlib_export) {
		Com_Printf( S_COLOR_RED "ERROR: Failed to initialize botlib for client\n" );
	}
}

/*
====================
CL_InitUI
====================
*/
#define UI_OLD_API_VERSION	4

void CL_InitUI( void ) {
	Com_Printf("=== CL_InitUI CALLED ===\n");

	// Initialize botlib for UI VM script parsing
	CL_InitBotLib();

	// Initialize UI virtual machine based on vm_ui CVAR
	cvar_t *vm_ui = Cvar_Get( "vm_ui", "2", CVAR_ARCHIVE | CVAR_PROTECTED );
	vmInterpret_t interpret = VM_SelectInterpret( "vm_ui", VMI_NATIVE, qfalse );

	// Try to load UI VM module
		uivm = VM_Create( VM_UI, CL_UISystemCalls, UI_DllSyscall, interpret );
	if ( !uivm ) {
		Com_Printf( S_COLOR_RED "WARNING: Failed to load UI VM (vm_ui = %d), falling back to no UI\n", vm_ui->integer );
		cls.uiStarted = qtrue;
		uivm = NULL;
		Com_Printf( "INFO: UI system initialized (VM loading failed - no UI modules available)\n" );
	} else {
		// VM loaded successfully
		cls.uiStarted = qtrue;
		Com_Printf( "UI VM loaded successfully (vm_ui = %d)\n", vm_ui->integer );
	}

	// UI_INIT is deferred until after renderer initialization.
	ui_initialized = qfalse;

	// Check if intro should be skipped
	cvar_t *skipIntro = Cvar_Get( "cl_skipIntro", "0", CVAR_ARCHIVE );
	if ( skipIntro->integer ) {
		Com_Printf( "Intro video skipped (cl_skipIntro = 1)\n" );
		return;
	}

	// Automatically play intro video based on mod and available formats
	const char *fs_game = Cvar_VariableString( "fs_game" );
	const char *baseName = NULL;

	// Determine base video name for this mod
	if ( !fs_game || !fs_game[0] || Q_stricmp( fs_game, "base" ) == 0 ) {
		// Base game - standard Quake 3 intro
		baseName = "video/idlogo";
	} else if ( Q_stricmp( fs_game, "openarena" ) == 0 ) {
		// OpenArena - OpenArena specific intro
		baseName = "video/openarena_intro";
	} else if ( Q_stricmp( fs_game, "mymod" ) == 0 ) {
		// MyMod - custom intro
		baseName = "video/mymod_intro";
	} else {
		// Other mods - try generic intro
		baseName = "video/intro";
	}

	// Try different formats in order of preference
	const char *formats_default[] = { ".roq", ".webm", ".ogv", ".ogg" };
	const char *formats_openarena[] = { ".ogv", ".webm", ".roq", ".ogg" };
	const char **formats = formats_default;
	int formatCount = 4;
	if ( fs_game && !Q_stricmp( fs_game, "openarena" ) ) {
		formats = formats_openarena;
	}
	static char selectedVideoFile[256] = {0};
	const char *videoFile = NULL;
	int i;

	for ( i = 0; i < formatCount; i++ ) {
		char testFile[256];
		Com_sprintf( testFile, sizeof(testFile), "%s%s", baseName, formats[i] );
		if ( FS_FileExists( testFile ) ) {
			Q_strncpyz( selectedVideoFile, testFile, sizeof(selectedVideoFile) );
			videoFile = selectedVideoFile;
			break;
		}
	}

	// Fallback to any available intro file
	if ( !videoFile ) {
		const char *fallbackFiles[] = {
			"video/intro.ogv", "video/intro.webm", "video/intro.roq",
			"video/idlogo.ogv", "video/idlogo.webm", "video/idlogo.roq"
		};

		for ( i = 0; i < 6; i++ ) {
			if ( FS_FileExists( fallbackFiles[i] ) ) {
				videoFile = fallbackFiles[i];
				break;
			}
		}
	}

	if ( videoFile ) {
		extern qboolean re_initialized;
		if ( re_initialized ) {
			Com_Printf( "Playing intro video: %s\n", videoFile );
			Cbuf_AddText( va( "cinematic %s 2\n", videoFile ) ); // 2 = loop mode
		} else {
			Com_Printf( "Intro video '%s' queued (renderer not initialized yet)\n", videoFile );
			CL_QueueIntroVideo( videoFile );
		}
	} else {
		Com_Printf( "No intro video found for mod '%s'\n", fs_game && fs_game[0] ? fs_game : "base" );
	}
}

qboolean UI_usesUniqueCDKey( void ) {
	// UI is disabled, so no unique CD keys
	return qfalse;
}


/*
====================
UI_GameCommand

See if the current console command is claimed by the ui
====================
*/
qboolean UI_GameCommand( void ) {
	if ( !uivm ) {
		return qfalse;
	}

	return VM_Call( uivm, 1, UI_CONSOLE_COMMAND, cls.realtime );
}
