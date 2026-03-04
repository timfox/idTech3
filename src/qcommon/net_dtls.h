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

#ifndef NET_DTLS_H
#define NET_DTLS_H

#include "q_shared.h"

#ifdef USE_DTLS

/*
 * DTLS (Datagram TLS) encryption for game traffic.
 * Optional layer that encrypts/decrypts packets when net_dtls cvar is enabled.
 * Uses OpenSSL DTLS. Sessions are keyed by remote address.
 */

void NET_DTLS_Init( void );
void NET_DTLS_Shutdown( void );

/* Encrypt data for sending. Returns encrypted length or -1 on error. */
int NET_DTLS_Encrypt( const netadr_t *to, const byte *data, int len, byte *out, int outMax );

/* Decrypt received data. Returns decrypted length or -1 on error/drop. */
int NET_DTLS_Decrypt( const netadr_t *from, const byte *data, int len, byte *out, int outMax );

qboolean NET_DTLS_IsEnabled( void );

#else

#define NET_DTLS_Init()           ((void)0)
#define NET_DTLS_Shutdown()       ((void)0)
#define NET_DTLS_IsEnabled()      (qfalse)

#endif /* USE_DTLS */

#endif /* NET_DTLS_H */
