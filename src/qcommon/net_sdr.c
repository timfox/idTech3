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

#ifndef USE_STEAM_NETWORKING
/* This file is compiled only when USE_STEAM_NETWORKING is defined. */
#else

#include "q_shared.h"
#include "qcommon.h"
#include "net_ip.h"
#include "net_sdr.h"

static cvar_t *net_sdr;
static qboolean sdr_initialized = qfalse;

void NET_SDR_Init( void )
{
	net_sdr = Cvar_Get( "net_sdr", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( net_sdr, "Use Steam Datagram Relay for game traffic (0=off, 1=on). Requires Steamworks SDK." );

	/* TODO: Initialize Steam Networking Sockets (ISteamNetworkingSockets) when
	 * Steamworks SDK is available. Requires SteamAPI_Init() and
	 * SteamNetworkingSockets() interface. */
	sdr_initialized = qtrue;
	Com_Printf( "Steam SDR: stub initialized (net_sdr=%d, full integration TODO)\n", net_sdr->integer );
}

void NET_SDR_Shutdown( void )
{
	if ( !sdr_initialized )
		return;

	/* TODO: Close Steam Networking Sockets */
	sdr_initialized = qfalse;
	Com_Printf( "Steam SDR: shutdown\n" );
}

void NET_SDR_Frame( void )
{
	if ( !sdr_initialized )
		return;

	/* TODO: Poll Steam Networking Sockets for incoming messages */
}

qboolean NET_SDR_SendPacket( netsrc_t sock, int length, const void *data, const netadr_t *to )
{
	if ( !sdr_initialized || !net_sdr || !net_sdr->integer )
		return qfalse;

	/* TODO: Send via SteamNetworkingSockets()->SendMessageToConnection() when
	 * we have a connection. For now, fall through to UDP. */
	(void)sock;
	(void)length;
	(void)data;
	(void)to;
	return qfalse;
}

qboolean NET_SDR_IsActive( void )
{
	return sdr_initialized && net_sdr && net_sdr->integer;
}

qboolean NET_SDR_UseForAddress( const netadr_t *adr )
{
	if ( !NET_SDR_IsActive() )
		return qfalse;

	/* TODO: Check if address is a Steam SDR connection. */
	(void)adr;
	return qfalse;
}

#endif /* USE_STEAM_NETWORKING */
