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

#ifndef NET_SDR_H
#define NET_SDR_H

#include "q_shared.h"

#ifdef USE_STEAM_NETWORKING

/*
 * Steam Datagram Relay (SDR) integration.
 * Optional transport using Valve's Steam Networking Sockets (ISteamNetworkingSockets)
 * for NAT traversal, relay, and built-in encryption.
 *
 * When enabled via net_sdr cvar, game traffic can use SDR instead of raw UDP.
 * Requires Steamworks SDK with SteamNetworkingSockets.
 */

void NET_SDR_Init( void );
void NET_SDR_Shutdown( void );
void NET_SDR_Frame( void );

/* Send via SDR when active; otherwise falls through to normal UDP. */
qboolean NET_SDR_SendPacket( netsrc_t sock, int length, const void *data, const netadr_t *to );

/* Check if SDR is active and we should use it for this address. */
qboolean NET_SDR_IsActive( void );
qboolean NET_SDR_UseForAddress( const netadr_t *adr );

#else

#define NET_SDR_Init()             ((void)0)
#define NET_SDR_Shutdown()         ((void)0)
#define NET_SDR_Frame()            ((void)0)
#define NET_SDR_SendPacket(a,b,c,d)  (qfalse)
#define NET_SDR_IsActive()         (qfalse)
#define NET_SDR_UseForAddress(a)    (qfalse)

#endif /* USE_STEAM_NETWORKING */

#endif /* NET_SDR_H */
