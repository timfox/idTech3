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
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#define DTLS_NONCE_LEN 12
#define DTLS_TAG_LEN 16
#define DTLS_OVERHEAD (4 + DTLS_NONCE_LEN + DTLS_TAG_LEN)  /* 32 bytes */

static cvar_t *net_dtls;
static cvar_t *net_dtls_key;
static qboolean dtls_initialized = qfalse;
static byte dtls_key[32];  /* AES-256 */
static qboolean dtls_key_valid = qfalse;

static void derive_key( void )
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if ( !ctx ) return;
	if ( EVP_DigestInit_ex( ctx, EVP_sha256(), NULL ) != 1 ) {
		EVP_MD_CTX_free( ctx );
		return;
	}
	EVP_DigestUpdate( ctx, net_dtls_key->string, strlen( net_dtls_key->string ) );
	EVP_DigestFinal_ex( ctx, dtls_key, NULL );
	EVP_MD_CTX_free( ctx );
	dtls_key_valid = qtrue;
#else
	SHA256_CTX ctx;
	SHA256_Init( &ctx );
	SHA256_Update( &ctx, (const unsigned char *)net_dtls_key->string, strlen( net_dtls_key->string ) );
	SHA256_Final( dtls_key, &ctx );
	dtls_key_valid = qtrue;
#endif
}

void NET_DTLS_Init( void )
{
	net_dtls = Cvar_Get( "net_dtls", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( net_dtls, "Enable DTLS encryption for game traffic (0=off, 1=on). Requires net_dtls_key." );
	net_dtls_key = Cvar_Get( "net_dtls_key", "", CVAR_ARCHIVE_ND | CVAR_PROTECTED );
	Cvar_SetDescription( net_dtls_key, "Pre-shared key for DTLS encryption. Must match on client and server." );

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	OPENSSL_init_crypto( OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL );
#else
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
#endif

	if ( RAND_status() != 1 ) {
		Com_Printf( "DTLS: Warning - insufficient entropy for secure random\n" );
	}

	dtls_initialized = qtrue;
	dtls_key_valid = qfalse;
	if ( net_dtls_key->string[0] ) {
		derive_key();
	}

	Com_Printf( "DTLS: initialized (net_dtls=%d, key=%s)\n",
		net_dtls->integer, dtls_key_valid ? "set" : "not set" );
}

void NET_DTLS_Shutdown( void )
{
	if ( !dtls_initialized )
		return;

	Com_Memset( dtls_key, 0, sizeof( dtls_key ) );
	dtls_key_valid = qfalse;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	EVP_cleanup();
	ERR_free_strings();
#endif

	dtls_initialized = qfalse;
	Com_Printf( "DTLS: shutdown\n" );
}

qboolean NET_DTLS_IsEnabled( void )
{
	return dtls_initialized && net_dtls && net_dtls->integer && dtls_key_valid;
}

/*
 * Encrypt with AES-256-GCM. Format: magic(4) + nonce(12) + ciphertext + tag(16).
 */
int NET_DTLS_Encrypt( const netadr_t *to, const byte *data, int len, byte *out, int outMax )
{
	if ( !NET_DTLS_IsEnabled() || len <= 0 || outMax < len + DTLS_OVERHEAD )
		return -1;

	/* Re-derive key if cvar changed */
	if ( !dtls_key_valid && net_dtls_key->string[0] )
		derive_key();
	if ( !dtls_key_valid )
		return -1;

	byte nonce[DTLS_NONCE_LEN];
	if ( RAND_bytes( nonce, DTLS_NONCE_LEN ) != 1 )
		return -1;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if ( !ctx ) return -1;

	if ( EVP_EncryptInit_ex( ctx, EVP_aes_256_gcm(), NULL, NULL, NULL ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	if ( EVP_EncryptInit_ex( ctx, NULL, NULL, dtls_key, nonce ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}

	int outlen = 0;
	if ( EVP_EncryptUpdate( ctx, out + 4 + DTLS_NONCE_LEN, &outlen, data, len ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	int taglen = 0;
	if ( EVP_EncryptFinal_ex( ctx, NULL, &taglen ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	if ( EVP_CIPHER_CTX_ctrl( ctx, EVP_CTRL_GCM_GET_TAG, DTLS_TAG_LEN, out + 4 + DTLS_NONCE_LEN + len ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	EVP_CIPHER_CTX_free( ctx );
#else
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init( &ctx );
	if ( EVP_EncryptInit_ex( &ctx, EVP_aes_256_gcm(), NULL, NULL, NULL ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	if ( EVP_EncryptInit_ex( &ctx, NULL, NULL, dtls_key, nonce ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	int outlen = 0;
	if ( EVP_EncryptUpdate( &ctx, out + 4 + DTLS_NONCE_LEN, &outlen, data, len ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	int taglen = 0;
	if ( EVP_EncryptFinal_ex( &ctx, NULL, &taglen ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	if ( EVP_CIPHER_CTX_ctrl( &ctx, EVP_CTRL_GCM_GET_TAG, DTLS_TAG_LEN, out + 4 + DTLS_NONCE_LEN + len ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	EVP_CIPHER_CTX_cleanup( &ctx );
#endif

	*(int32_t *)out = DTLS_MAGIC;
	Com_Memcpy( out + 4, nonce, DTLS_NONCE_LEN );


	return 4 + DTLS_NONCE_LEN + len + DTLS_TAG_LEN;
}

/*
 * Decrypt AES-256-GCM. Returns decrypted length or -1 on error.
 */
int NET_DTLS_Decrypt( const netadr_t *from, const byte *data, int len, byte *out, int outMax )
{
	if ( !NET_DTLS_IsEnabled() || len <= DTLS_OVERHEAD || outMax < len - DTLS_OVERHEAD )
		return -1;

	if ( *(const int32_t *)data != DTLS_MAGIC )
		return -1;

	/* Re-derive key if cvar changed */
	if ( !dtls_key_valid && net_dtls_key->string[0] )
		derive_key();
	if ( !dtls_key_valid )
		return -1;

	const byte *nonce = data + 4;
	const byte *cipher = data + 4 + DTLS_NONCE_LEN;
	int cipherlen = len - 4 - DTLS_NONCE_LEN - DTLS_TAG_LEN;
	if ( cipherlen <= 0 )
		return -1;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if ( !ctx ) return -1;

	if ( EVP_DecryptInit_ex( ctx, EVP_aes_256_gcm(), NULL, NULL, NULL ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	if ( EVP_DecryptInit_ex( ctx, NULL, NULL, dtls_key, nonce ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}

	int outlen = 0;
	if ( EVP_DecryptUpdate( ctx, out, &outlen, cipher, cipherlen ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	if ( EVP_CIPHER_CTX_ctrl( ctx, EVP_CTRL_GCM_SET_TAG, DTLS_TAG_LEN, (void *)( (byte *)data + 4 + DTLS_NONCE_LEN + cipherlen ) ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;
	}
	if ( EVP_DecryptFinal_ex( ctx, NULL, &outlen ) != 1 ) {
		EVP_CIPHER_CTX_free( ctx );
		return -1;  /* auth failed, drop packet */
	}
	EVP_CIPHER_CTX_free( ctx );
#else
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init( &ctx );
	if ( EVP_DecryptInit_ex( &ctx, EVP_aes_256_gcm(), NULL, NULL, NULL ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	if ( EVP_DecryptInit_ex( &ctx, NULL, NULL, dtls_key, nonce ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	int outlen = 0;
	if ( EVP_DecryptUpdate( &ctx, out, &outlen, cipher, cipherlen ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	if ( EVP_CIPHER_CTX_ctrl( &ctx, EVP_CTRL_GCM_SET_TAG, DTLS_TAG_LEN, (void *)( (byte *)data + 4 + DTLS_NONCE_LEN + cipherlen ) ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	if ( EVP_DecryptFinal_ex( &ctx, NULL, &outlen ) != 1 ) {
		EVP_CIPHER_CTX_cleanup( &ctx );
		return -1;
	}
	EVP_CIPHER_CTX_cleanup( &ctx );
#endif

	return cipherlen;
}

#endif /* USE_DTLS */
