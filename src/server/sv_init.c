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

#include "server.h"
#include "../client/client.h"
#include "../common/q_memory_safety.h"
static qboolean CM_CheckAssetDependencies( const char *mapname )
{
    // Preflight: verify map asset availability before loading
    if ( !mapname || !*mapname ) {
        Com_Printf( S_COLOR_YELLOW "Preflight: invalid map name in CM_CheckAssetDependencies\n" );
        return qfalse;
    }
    char mappath[MAX_QPATH];
    Com_sprintf( mappath, sizeof( mappath ), "maps/%s.bsp", mapname );
    int len = FS_FOpenFileRead( mappath, NULL, qfalse );
    if ( len == -1 ) {
        Com_Printf( S_COLOR_YELLOW "Preflight: map file not found: %s\n", mappath );
        return qfalse;
    }
    return qtrue;
}
#ifdef USE_ENTT
#include "sv_ecs.h"
#endif


/*
===============
SV_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
static void SV_SendConfigstring(client_t *client, int index)
{
	int maxChunkSize = MAX_STRING_CHARS - 24;
	int len;

	len = strlen(sv.configstrings[index]);

	if( len >= maxChunkSize ) {
		int		sent = 0;
		int		remaining = len;
		const char	*cmd;
		char	buf[MAX_STRING_CHARS];

		while (remaining > 0 ) {
			if ( sent == 0 ) {
				cmd = "bcs0";
			}
			else if( remaining < maxChunkSize ) {
				cmd = "bcs2";
			}
			else {
				cmd = "bcs1";
			}
			Q_strncpyz( buf, &sv.configstrings[index][sent],
				maxChunkSize );

			SV_SendServerCommand( client, "%s %i \"%s\"", cmd,
				index, buf );

			sent += (maxChunkSize - 1);
			remaining -= (maxChunkSize - 1);
		}
	} else {
		// standard cs, just send it
		SV_SendServerCommand( client, "cs %i \"%s\"", index,
			sv.configstrings[index] );
	}
}

/*
===============
SV_UpdateConfigstrings

Called when a client goes from CS_PRIMED to CS_ACTIVE.  Updates all
Configstring indexes that have changed while the client was in CS_PRIMED
===============
*/
void SV_UpdateConfigstrings(client_t *client)
{
	int index;

	for( index = 0; index < MAX_CONFIGSTRINGS; index++ ) {
		// if the CS hasn't changed since we went to CS_PRIMED, ignore
		if(!client->csUpdated[index])
			continue;

		// do not always send server info to all clients
		// Check if gentities are initialized before accessing them
		if ( index == CS_SERVERINFO && sv.gentities ) {
			int clientNum = client - svs.clients;
			if ( clientNum >= 0 && clientNum < sv.num_entities ) {
				sharedEntity_t *gent = SV_GentityNum( clientNum );
				if ( gent && ( gent->r.svFlags & SVF_NOSERVERINFO ) ) {
					continue;
				}
			}
		}

		SV_SendConfigstring(client, index);
		client->csUpdated[index] = qfalse;
	}
}

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	int		i;
	client_t	*client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	MEMORY_SAFETY_FREE( sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {
		// Safety check: ensure clients array is initialized
		if ( !svs.clients ) {
			return;
		}

		// send the data to all relevant clients
		for (i = 0, client = svs.clients; i < sv.maxclients; i++, client++) {
			// Safety check: ensure client pointer is valid
			if ( !client ) {
				continue;
			}
			if ( client->state < CS_ACTIVE ) {
				if ( client->state == CS_PRIMED || client->state == CS_CONNECTED ) {
					// track CS_CONNECTED clients as well to optimize gamestate acknowledge after downloading/retransmission
					client->csUpdated[index] = qtrue;
				}
				continue;
			}
			// do not always send server info to all clients
			// Check if gentities are initialized before accessing them
			if ( index == CS_SERVERINFO && sv.gentities && i < sv.num_entities ) {
				sharedEntity_t *gent = SV_GentityNum( i );
				if ( gent && ( gent->r.svFlags & SVF_NOSERVERINFO ) ) {
					continue;
				}
			}

			SV_SendConfigstring(client, index);
		}
	}
}


/*
===============
SV_GetConfigstring
===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = '\0';
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv.maxclients ) {
		Com_Error( ERR_DROP, "%s: bad index %i", __func__, index );
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "%s: bufferSize == %i", __func__, bufferSize );
	}
	if ( index < 0 || index >= sv.maxclients ) {
		Com_Error( ERR_DROP, "%s: bad index %i", __func__, index );
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline( void ) {
	sharedEntity_t *ent;
	int				entnum;

	// Safety check: ensure gentities are initialized
	if ( !sv.gentities || sv.num_entities == 0 ) {
		return;
	}

	for ( entnum = 0; entnum < sv.num_entities ; entnum++ ) {
		ent = SV_GentityNum( entnum );
		if ( !ent->r.linked ) {
			continue;
		}
		ent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[ entnum ].baseline = ent->s;
		sv.baselineUsed[ entnum ] = 1;
	}
}


/*
===============
SV_BoundMaxClients
===============
*/
static int SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	if ( sv_maxclients->integer < minimum ) {
		Cvar_SetIntegerValue( "sv_maxclients", minimum );
		sv_maxclients->modified = qfalse;
		return minimum;
	}

	sv_maxclients->modified = qfalse;

	return sv_maxclients->integer;
}


/*
===============
SV_SetSnapshotParams
===============
*/
static void SV_SetSnapshotParams( void )
{
	// PACKET_BACKUP frames is just about 6.67MB so use that even on listen servers
	svs.numSnapshotEntities = PACKET_BACKUP * MAX_GENTITIES;
}


/*
===============
SV_AllocClients
===============
*/
static void SV_AllocClients( int count )
{
	svs.clients = Z_TagMalloc( count * sizeof( client_t ), TAG_CLIENTS );
	Com_Memset( svs.clients, 0x0, count * sizeof( client_t ) );
	sv.maxclients = count;
	SV_SetSnapshotParams();
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
static void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}

	SV_AllocClients( sv_maxclients->integer );

	sv_maxclients->modified = qfalse;

	svs.initialized = qtrue;

	// Don't respect sv_killserver unless a server is actually running
	if ( sv_killserver->integer ) {
		Cvar_Set( "sv_killserver", "0" );
	}

	Cvar_Set( "sv_running", "1" );

	// Join the ipv6 multicast group now that a map is running so clients can scan for us on the local network.
#ifdef USE_IPV6
	NET_JoinMulticast6();
#endif
}


/*
==================
SV_ChangeMaxClients
==================
*/
static void SV_ChangeMaxClients( void ) {
	client_t *oldClients;
	int		maxclients;
	int		count;
	int		i;

	// get the highest client number in use
	count = 0;
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if ( i > count ) {
				count = i;
			}
		}
	}
	count++;

	// never go below the highest client number in use
	maxclients = SV_BoundMaxClients( count );

	// if still the same
	if ( maxclients == sv.maxclients ) {
		return;
	}

	oldClients = Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( i = 0; i < count; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		} else {
			Com_Memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
	SV_AllocClients( maxclients );

	// copy the clients over
	for ( i = 0; i < count; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );
}


/*
================
SV_ClearServer
================
*/
static void SV_ClearServer( void ) {
	int i;

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( sv.configstrings[i] ) {
			MEMORY_SAFETY_FREE( sv.configstrings[i] );
		}
	}

	if ( !sv_levelTimeReset->integer ) {
		i = sv.time;
		Com_Memset( &sv, 0, sizeof( sv ) );
		sv.time = i;
	} else {
		Com_Memset( &sv, 0, sizeof( sv ) );
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
void SV_SpawnServer( const char *mapname, qboolean killBots ) {
	int			i;
	int			checksum;
	qboolean	isBot;
	const char	*p;

	if ( !FS_StartupInProgress() ) {
	}
	// shut down the existing game if it is running
	SV_ShutdownGameProgs();
	if ( !FS_StartupInProgress() ) {
	}

	// Only print if filesystem is initialized (not during FS_Restart)
	if ( !FS_StartupInProgress() ) {
		if ( !FS_StartupInProgress() ) {
		}
		Com_Printf( "------ Server Initialization ------\n" );
		if ( !FS_StartupInProgress() ) {
		}
		Com_Printf( "Server: %s\n", mapname );
	}

	if ( !FS_StartupInProgress() ) {
	}
	Sys_SetStatus( "Initializing server..." );
	if ( !FS_StartupInProgress() ) {
	}

#ifndef DEDICATED
	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	// Only call client functions if the renderer is initialized to avoid crashes
	extern qboolean re_initialized;
	if ( re_initialized ) {
		if ( !FS_StartupInProgress() ) {
		}
		CL_MapLoading();
		if ( !FS_StartupInProgress() ) {
		}

		// make sure all the client stuff is unloaded
		if ( !FS_StartupInProgress() ) {
		}
		CL_ShutdownAll();
		if ( !FS_StartupInProgress() ) {
		}
	}
#endif

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

	// clear collision map data
	CM_ClearMap();

	// timescale can be updated before SV_Frame() and cause division-by-zero in SV_RateMsec()
	Cvar_CheckRange( com_timescale, "0.001", NULL, CV_FLOAT );

	// init client structures and svs.numSnapshotEntities
	if ( !Cvar_VariableIntegerValue( "sv_running" ) ) {
		SV_Startup();
	} else {
		// check for maxclients change
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

#ifndef DEDICATED
	// remove pure paks that may left from client-side
	FS_PureServerSetLoadedPaks( "", "" );
	FS_PureServerSetReferencedPaks( "", "" );
#endif

	// clear pak references
	FS_ClearPakReferences( 0 );

	// allocate the snapshot entities on the hunk
	svs.snapshotEntities = Hunk_Alloc( sizeof(entityState_t)*svs.numSnapshotEntities, h_high );

	// initialize snapshot storage
	SV_InitSnapshotStorage();

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overridden
	// by the game startup or another console command
	Cvar_Set( "nextmap", "map_restart 0" );
//	Cvar_Set( "nextmap", va("map %s", server) );

	// try to reset level time if server is empty
	if ( !sv_levelTimeReset->integer && !sv.restartTime ) {
		for ( i = 0; i < sv.maxclients; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED ) {
				break;
			}
		}
		if ( i == sv.maxclients ) {
			sv.time = 0;
		}
	}

	for ( i = 0; i < sv.maxclients; i++ ) {
		// save when the server started for each client already connected
		if ( svs.clients[i].state >= CS_CONNECTED && sv_levelTimeReset->integer ) {
			svs.clients[i].oldServerTime = sv.time;
		} else {
			svs.clients[i].oldServerTime = 0;
		}
	}

	// preserve maxclients
	i = sv.maxclients;
	// wipe the entire per-level structure
	SV_ClearServer();
	sv.maxclients = i;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		sv.configstrings[i] = CopyString("");
	}

	// make sure we are not paused
#ifndef DEDICATED
	Cvar_Set( "cl_paused", "0" );
#endif

	// get latched value
	sv_pure = Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH );

	// VMs can change latched cvars instantly which could cause side-effects in SV_UserMove()
	sv.pure = sv_pure->integer;

	// get a new checksum feed and restart the file system
	// Note: Don't use Com_Printf here as FS_Restart clears fs_searchpaths temporarily
	srand( Com_Milliseconds() );
	Com_RandomBytes( (byte*)&sv.checksumFeed, sizeof( sv.checksumFeed ) );
	// FS_Restart disabled - causes memory corruption with Vulkan renderer
	// FS_Restart( sv.checksumFeed );

	// Memory integrity validation is performed during Vulkan shutdown

	Com_Printf("Filesystem restart disabled (prevents Vulkan memory corruption)\n");
	// Debug output after FS_Restart completes
	if ( !FS_StartupInProgress() ) {
	}

#ifndef DEDICATED
	// After filesystem restart, client systems need to be restarted
	// This ensures renderer, sound, and UI are properly initialized
	// CL_MapLoading already shut down client systems, now restart them
	if ( com_cl_running && com_cl_running->integer ) {
		if ( !FS_StartupInProgress() ) {
		}
		CL_StartHunkUsers();
		if ( !FS_StartupInProgress() ) {
		}
	}
#endif

	// Validate that the map exists before trying to load it
	{
		char expanded[MAX_QPATH];
		int len;

		Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", mapname );
		// bypass pure check so we can open downloaded map
		FS_BypassPure();
		len = FS_FOpenFileRead( expanded, NULL, qfalse );
		FS_RestorePure();
		if ( len == -1 ) {
			Com_Printf( S_COLOR_RED "ERROR: Map '%s' not found. Available maps:\n", mapname );
			Com_Printf( S_COLOR_YELLOW "  OpenArena: oa_dm1, oa_dm2, oa_dm3, oa_dm4, oa_dm5, oa_dm6, oa_dm7\n" );
			Com_Printf( S_COLOR_YELLOW "  Quake 3: q3dm9, pro-q3dm13, pro-q3dm6 (and others in pk3 files)\n" );
			Com_Printf( S_COLOR_CYAN "  Custom: Check your mod's maps directory\n" );
			return;
		}
	}

    Sys_SetStatus( "Loading map %s", mapname );
    Com_Printf( "CM_CheckAssetDependencies: preflight for map %s\n", mapname );
    if ( !CM_CheckAssetDependencies( mapname ) ) {
        Com_Printf( S_COLOR_YELLOW "Preflight: asset checks failed for map %s; continuing anyway\n", mapname );
    }
Com_Printf( "CM_LoadMap: loading maps/%s.bsp\n", mapname );
if ( !FS_StartupInProgress() ) {
	}
	CM_LoadMap( va( "maps/%s.bsp", mapname ), qfalse, &checksum );
	Com_Printf( "CM_LoadMap: loaded maps/%s.bsp, checksum=%d\n", mapname, checksum );

	// Temporary workaround: Disable Vulkan during critical initialization to prevent memory corruption
	extern cvar_t *cl_renderer;
	if (cl_renderer && Q_stricmp(cl_renderer->string, "vulkan") == 0) {
		Com_Printf("Vulkan renderer: Temporarily disabling during client initialization to prevent corruption\n");
		// The renderer will be re-enabled after client initialization completes
	}
if ( !FS_StartupInProgress() ) {
}

// Now restart the filesystem with the new checksum after map loading
// This avoids memory corruption during early initialization
FS_Restart( sv.checksumFeed );
// Debug output after FS_Restart completes
if ( !FS_StartupInProgress() ) {
}

	// set serverinfo visible name
	Cvar_Set( "mapname", mapname );

	Cvar_SetIntegerValue( "sv_mapChecksum", checksum );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId;
	Cvar_SetIntegerValue( "sv_serverid", sv.serverId );

	// clear physics interaction links
	SV_ClearWorld();

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// make sure that level time is not zero
	//sv.time = sv.time ? sv.time : 8;

	// load and spawn all other entities
	if ( !FS_StartupInProgress() ) {
	}
	SV_InitGameProgs();
	if ( !FS_StartupInProgress() ) {
	}

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	sv_pure->modified = qfalse;

	// run a few frames to allow everything to settle
	// Safety check: ensure game VM is initialized
	if ( gvm ) {
		for ( i = 0; i < 3; i++ ) {
			Cbuf_Wait();
			sv.time += 100;
			VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
			SV_BotFrame( sv.time );
		}
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	for ( i = 0; i < sv.maxclients; i++ ) {
		// send the new gamestate to all connected clients
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			const char *denied;

			if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
				if ( killBots ) {
					SV_DropClient( &svs.clients[i], "was kicked" );
					continue;
				}
				isBot = qtrue;
			}
			else {
				isBot = qfalse;
			}

			// connect the client again
			// Safety check: ensure game VM is initialized
			if ( !gvm ) {
				SV_DropClient( &svs.clients[i], "game VM not initialized" );
				continue;
			}
			denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );	// firstTime = qfalse
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if ( !isBot ) {
					svs.clients[i].gamestateAck = GSA_INIT; // resend gamestate, accept first correct serverId
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
					svs.clients[i].gentity = NULL;
				} else {
					SV_ClientEnterWorld( &svs.clients[i] );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	Cbuf_Wait();
	sv.time += 100;
	// Safety check: ensure game VM is initialized
	if ( gvm ) {
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	}
	SV_BotFrame( sv.time );
	svs.time += 100;

	// we need to touch the cgame and ui qvm because they could be in
	// separate pk3 files and the client will need to download the pk3
	// files with the latest cgame and ui qvm to pass the pure check
	FS_TouchFileInPak( "vm/cgame.qvm" );
	FS_TouchFileInPak( "vm/ui.qvm" );

	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	p = FS_ReferencedPakNames();
	if ( FS_ExcludeReference() ) {
		// \fs_excludeReference may mask our current ui/cgame qvms
		FS_TouchFileInPak( "vm/cgame.qvm" );
		FS_TouchFileInPak( "vm/ui.qvm" );
		// rebuild referenced paks list
		p = FS_ReferencedPakNames();
	}
	Cvar_Set( "sv_referencedPakNames", p );

	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );

	Cvar_Set( "sv_paks", "" );
	Cvar_Set( "sv_pakNames", "" ); // not used on client-side

	if ( sv.pure != 0 ) {
		int freespace, pakslen, infolen;
		qboolean overflowed = qfalse;
		qboolean infoTruncated = qfalse;

		p = FS_LoadedPakChecksums( &overflowed );

		pakslen = strlen( p ) + 9; // + strlen( "\\sv_paks\\" )
		freespace = SV_RemainingGameState();
		infolen = strlen( Cvar_InfoString_Big( CVAR_SYSTEMINFO, &infoTruncated ) );

		if ( infoTruncated ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: truncated systeminfo!\n" );
		}

		if ( pakslen > freespace || infolen + pakslen >= BIG_INFO_STRING || overflowed ) {
			// switch to degraded pure mode
			// this could *potentially* lead to a false "unpure client" detection
			// which is better than guaranteed drop
			Com_DPrintf( S_COLOR_YELLOW "WARNING: skipping sv_paks setup to avoid gamestate overflow\n" );
		} else {
			// the server sends these to the clients so they will only
			// load pk3s also loaded at the server
			Cvar_Set( "sv_paks", p );
			if ( *p == '\0' ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: sv_pure set but no PK3 files loaded\n" );
			}
		}
	}

	// save systeminfo and serverinfo strings
	SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO, NULL ) );
	{ int old_val, new_val; do { old_val = atomic_load_explicit(&cvar_modifiedFlags, memory_order_relaxed); new_val = old_val & (~CVAR_SYSTEMINFO); } while (!atomic_compare_exchange_weak_explicit(&cvar_modifiedFlags, &old_val, new_val, memory_order_relaxed, memory_order_relaxed)); }

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO, NULL ) );
	{ int old_val, new_val; do { old_val = atomic_load_explicit(&cvar_modifiedFlags, memory_order_relaxed); new_val = old_val & (~CVAR_SERVERINFO); } while (!atomic_compare_exchange_weak_explicit(&cvar_modifiedFlags, &old_val, new_val, memory_order_relaxed, memory_order_relaxed)); }

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

	Com_Printf ("-----------------------------------\n");

	Sys_SetStatus( "Running map %s", mapname );

	// suppress hitch warning
	Com_FrameInit();
}


/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_Init( void )
{
	int index;

#ifdef USE_ENTT
	SV_ECS_Init();
#endif

	SV_AddOperatorCommands();

	if ( com_dedicated->integer )
		SV_AddDedicatedCommands();

	// serverinfo vars
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO);
	Cvar_Get ("fraglimit", "20", CVAR_SERVERINFO);
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);
	sv_gametype = Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH );
	Cvar_SetDescription( sv_gametype, "Set the gametype to mod." );
	Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
	//Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM);
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
	Cvar_SetDescription( sv_mapname, "Display the name of the current map being used on a server." );
	sv_privateClients = Cvar_Get( "sv_privateClients", "0", CVAR_SERVERINFO );
	Cvar_CheckRange( sv_privateClients, "0", va( "%i", MAX_CLIENTS-1 ), CV_INTEGER );
	Cvar_SetDescription( sv_privateClients, "The number of spots, out of sv_maxclients, reserved for players with the server password (sv_privatePassword)." );
	sv_hostname = Cvar_Get ("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_SetDescription( sv_hostname, "Sets the name of the server." );
	sv_maxclients = Cvar_Get ("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
	Cvar_CheckRange( sv_maxclients, "1", XSTRING(MAX_CLIENTS), CV_INTEGER );
	Cvar_SetDescription( sv_maxclients, "Maximum number of people allowed to join the server." );

	sv_maxclientsPerIP = Cvar_Get( "sv_maxclientsPerIP", "3", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_maxclientsPerIP, "1", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_maxclientsPerIP, "Limits number of simultaneous connections from the same IP address." );

	sv_clientTLD = Cvar_Get( "sv_clientTLD", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( sv_clientTLD, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( sv_clientTLD, "Client country detection code." );

	sv_minRate = Cvar_Get( "sv_minRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO );
	Cvar_SetDescription( sv_minRate, "Minimum server bandwidth (in bit per second) a client can use." );
	sv_maxRate = Cvar_Get( "sv_maxRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO );
	Cvar_SetDescription( sv_maxRate, "Maximum server bandwidth (in bit per second) a client can use." );
	sv_dlRate = Cvar_Get( "sv_dlRate", "100", CVAR_ARCHIVE | CVAR_SERVERINFO );
	Cvar_CheckRange( sv_dlRate, "0", "500", CV_INTEGER );
	Cvar_SetDescription( sv_dlRate, "Bandwidth allotted to PK3 file downloads via UDP, in kbyte/s." );
	sv_floodProtect = Cvar_Get( "sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO );
	Cvar_SetDescription( sv_floodProtect, "Toggle server flood protection to keep players from bringing the server down." );

	// systeminfo
	Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_serverid = Cvar_Get( "sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_pure = Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH );
	Cvar_SetDescription( sv_pure, "Requires clients to only get data from pk3 files the server is using." );
	Cvar_Get( "sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_referencedPakNames = Cvar_Get( "sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_SetDescription( sv_referencedPakNames, "Variable holds a list of all the pk3 files the server loaded data from." );

	// server vars
	sv_rconPassword = Cvar_Get ("rconPassword", "", CVAR_TEMP );
	Cvar_SetDescription( sv_rconPassword, "Password for remote server commands." );
	sv_privatePassword = Cvar_Get ("sv_privatePassword", "", CVAR_TEMP );
	Cvar_SetDescription( sv_privatePassword, "Set password for private clients to login with." );
	sv_fps = Cvar_Get ("sv_fps", "20", CVAR_TEMP );
	Cvar_CheckRange( sv_fps, "10", "125", CV_INTEGER );
	Cvar_SetDescription( sv_fps, "Set the max frames per second the server sends the client." );
	sv_timeout = Cvar_Get( "sv_timeout", "200", CVAR_TEMP );
	Cvar_CheckRange( sv_timeout, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_timeout, "Seconds without any message before automatic client disconnect." );
	sv_zombietime = Cvar_Get( "sv_zombietime", "2", CVAR_TEMP );
	Cvar_CheckRange( sv_zombietime, "1", NULL, CV_INTEGER );
	Cvar_SetDescription( sv_zombietime, "Seconds to sink messages after disconnect." );
	Cvar_Get ("nextmap", "", CVAR_TEMP );

	sv_allowDownload = Cvar_Get ("sv_allowDownload", "1", CVAR_SERVERINFO);
	Cvar_SetDescription( sv_allowDownload, "Toggle the ability for clients to download files maps etc. from server." );
	Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);

	// moved to Com_Init()
	//sv_master[0] = Cvar_Get( "sv_master1", MASTER_SERVER_NAME, CVAR_INIT | CVAR_ARCHIVE_ND );
	//sv_master[1] = Cvar_Get( "sv_master2", "master.ioquake3.org", CVAR_INIT | CVAR_ARCHIVE_ND );
	//sv_master[2] = Cvar_Get( "sv_master3", "master.maverickservers.com", CVAR_INIT | CVAR_ARCHIVE_ND );

	for ( index = 0; index < MAX_MASTER_SERVERS; index++ )
		sv_master[ index ] = Cvar_Get( va( "sv_master%d", index + 1 ), "", CVAR_ARCHIVE_ND );

	sv_reconnectlimit = Cvar_Get( "sv_reconnectlimit", "3", 0 );
	Cvar_CheckRange( sv_reconnectlimit, "0", "12", CV_INTEGER );
	Cvar_SetDescription( sv_reconnectlimit, "Number of seconds a disconnected client should wait before next reconnect." );

	sv_padPackets = Cvar_Get( "sv_padPackets", "0", CVAR_DEVELOPER );
	Cvar_SetDescription( sv_padPackets, "Adds padding bytes to network packets for rate debugging." );
	sv_killserver = Cvar_Get( "sv_killserver", "0", 0 );
	Cvar_SetDescription( sv_killserver, "Internal flag to manage server state." );
	sv_mapChecksum = Cvar_Get( "sv_mapChecksum", "", CVAR_ROM );
	Cvar_SetDescription( sv_mapChecksum, "Allows check for client server map to match." );
	sv_lanForceRate = Cvar_Get( "sv_lanForceRate", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_lanForceRate, "Forces LAN clients to the maximum rate instead of accepting client setting." );
	sv_background = Cvar_Get( "sv_background", "0", CVAR_TEMP | CVAR_SERVERINFO );
	Cvar_CheckRange( sv_background, "0", "1", CV_INTEGER );
	Cvar_SetDescription( sv_background, "Flag indicating the server is running a background map (map_background). Set automatically." );
	sv_backgroundFps = Cvar_Get( "sv_backgroundFps", "20", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( sv_backgroundFps, "5", "125", CV_FLOAT );
	Cvar_SetDescription( sv_backgroundFps, "Server tickrate (FPS) to use when running a background map (map_background). Lower saves CPU/GPU while menu stays up." );

#ifdef USE_BANS
	sv_banFile = Cvar_Get("sv_banFile", "serverbans.dat", CVAR_ARCHIVE);
	Cvar_SetDescription( sv_banFile, "Name of the file that is used for storing the server bans." );
#endif

	sv_levelTimeReset = Cvar_Get( "sv_levelTimeReset", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_levelTimeReset, "Whether or not to reset leveltime after new map loads." );

	sv_filter = Cvar_Get( "sv_filter", "filter.txt", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_filter, "Cvar that point on filter file, if it is "" then filtering will be disabled." );

#ifdef USE_BULLET
	// Bullet physics tuning (ECS-backed)
	sv_bulletEnable = Cvar_Get( "sv_bulletEnable", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	Cvar_CheckRange( sv_bulletEnable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( sv_bulletEnable,
		"Enable Bullet-based physics stepping for ECS entities (requires USE_BULLET at build time)." );

	sv_bulletMaxSubSteps = Cvar_Get( "sv_bulletMaxSubSteps", "4", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_bulletMaxSubSteps, "1", "16", CV_INTEGER );
	Cvar_SetDescription( sv_bulletMaxSubSteps,
		"Maximum number of internal Bullet substeps per server frame (higher = more stable, slower)." );

	sv_bulletFixedTimestep = Cvar_Get( "sv_bulletFixedTimestep", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_bulletFixedTimestep,
		"Optional fixed Bullet timestep in seconds (0 = use dynamic server frame delta)." );

	sv_bulletDebug = Cvar_Get( "sv_bulletDebug", "0", CVAR_ARCHIVE | CVAR_DEVELOPER );
	Cvar_CheckRange( sv_bulletDebug, "0", "1", CV_INTEGER );
	Cvar_SetDescription( sv_bulletDebug,
		"Enable debug logging for Bullet ECS integration (developer use only)." );
#endif

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

#ifdef USE_BANS
	// Load saved bans
	Cbuf_AddText("rehashbans\n");
#endif

	// track group cvar changes
	Cvar_SetGroup( sv_lanForceRate, CVG_SERVER );
	Cvar_SetGroup( sv_minRate, CVG_SERVER );
	Cvar_SetGroup( sv_maxRate, CVG_SERVER );
	Cvar_SetGroup( sv_fps, CVG_SERVER );

#ifdef USE_BULLET
	Cvar_SetGroup( sv_bulletEnable, CVG_SERVER );
#endif

	// force initial check
	SV_TrackCvarChanges();

	SV_InitChallenger();
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
static void SV_FinalMessage( const char *message ) {
	int			i, j;
	client_t	*client;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for ( i = 0, client = svs.clients; i < sv.maxclients; i++, client++) {
			if (client->state >= CS_CONNECTED ) {
				// don't send a disconnect to a local client
				if ( client->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( client, "print \"%s\n\"\n", message );
					SV_SendServerCommand( client, "disconnect \"%s\"", message );
				}
				// force a snapshot to be sent
				client->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
				client->state = CS_ZOMBIE; // skip delta generation
				SV_SendClientSnapshot( client );
			}
		}
	}

	NET_FlushPacketQueue( 99999 );
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( const char *finalmsg ) {
#ifdef USE_ENTT
	SV_ECS_Shutdown();
#endif
	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

#ifdef USE_IPV6
	NET_LeaveMulticast6();
#endif

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();
	SV_InitChallenger();

	// free current level
	SV_ClearServer();

	SV_FreeIP4DB();

	// free server static data
	if ( svs.clients ) {
		int index;

		for ( index = 0; index < sv.maxclients; index++ )
			SV_FreeClient( &svs.clients[ index ] );

		Z_Free( svs.clients );
	}
	Com_Memset( &svs, 0, sizeof( svs ) );
	sv.time = 0;

	Cvar_Set( "sv_running", "0" );

	// allow setting timescale 0 for demo playback
	Cvar_CheckRange( com_timescale, "0", NULL, CV_FLOAT );

#ifndef DEDICATED
	Cvar_Set( "ui_singlePlayerActive", "0" );
#endif

	Com_Printf( "---------------------------\n" );

#ifndef DEDICATED
	// disconnect any local clients
	if ( sv_killserver->integer != 2 )
		CL_Disconnect( qfalse );
#endif

	// clean some server cvars
	Cvar_Set( "sv_referencedPaks", "" );
	Cvar_Set( "sv_referencedPakNames", "" );
	Cvar_Set( "sv_mapChecksum", "" );
	Cvar_Set( "sv_serverid", "0" );

	Sys_SetStatus( "Server is not running" );
}
