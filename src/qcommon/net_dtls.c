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

#ifndef USE_DTLS
/* This file is compiled only when USE_DTLS is defined. */
#else

#include "q_shared.h"
#include "qcommon.h"
#include "net_ip.h"
#include "net_dtls.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

static cvar_t *net_dtls;
static qboolean dtls_initialized = qfalse;

void NET_DTLS_Init( void )
{
	net_dtls = Cvar_Get( "net_dtls", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( net_dtls, "Enable DTLS encryption for game traffic (0=off, 1=on). Requires vid_restart to take effect." );

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	OPENSSL_init_ssl( OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL );
#else
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
#endif

	if ( RAND_status() != 1 ) {
		Com_Printf( "DTLS: Warning - insufficient entropy for secure random\n" );
	}

	dtls_initialized = qtrue;
	Com_Printf( "DTLS: initialized (net_dtls=%d)\n", net_dtls->integer );
}

void NET_DTLS_Shutdown( void )
{
	if ( !dtls_initialized )
		return;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	EVP_cleanup();
	ERR_free_strings();
#endif

	dtls_initialized = qfalse;
	Com_Printf( "DTLS: shutdown\n" );
}

qboolean NET_DTLS_IsEnabled( void )
{
	return dtls_initialized && net_dtls && net_dtls->integer;
}

/*
 * DTLS encrypt: when net_dtls is 0, caller passes through. When 1, we would
 * encrypt here. Full DTLS handshake and session management is TODO.
 * For now, passthrough (return -1 to indicate "not encrypted, use raw").
 */
int NET_DTLS_Encrypt( const netadr_t *to, const byte *data, int len, byte *out, int outMax )
{
	if ( !NET_DTLS_IsEnabled() || len <= 0 || outMax < len )
		return -1;

	/* TODO: full DTLS implementation - session per remote addr, handshake, etc. */
	/* For now, passthrough: caller will send raw when we return -1. */
	(void)to;
	(void)data;
	(void)len;
	(void)out;
	(void)outMax;
	return -1;
}

/*
 * DTLS decrypt: when net_dtls is 0, caller passes through. When 1, we would
 * decrypt here. Returns -1 to drop or pass through.
 */
int NET_DTLS_Decrypt( const netadr_t *from, const byte *data, int len, byte *out, int outMax )
{
	if ( !NET_DTLS_IsEnabled() || len <= 0 || outMax < len )
		return -1;

	/* TODO: full DTLS implementation */
	(void)from;
	(void)data;
	(void)len;
	(void)out;
	(void)outMax;
	return -1;
}

#endif /* USE_DTLS */
