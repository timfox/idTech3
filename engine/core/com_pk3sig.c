/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

pk3.sig sidecar format (v1 integrity):
  sha256=<hex digest of entire .pk3 file>
Verified when sv_pureSigned 1. See docs/MOD_SDK.md.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "com_pk3sig.h"

#include <stdio.h>
#include <string.h>

#if defined( USE_OPENSSL ) || defined( USE_DTLS )
#include <openssl/evp.h>
#endif

static qboolean Com_Pk3Sig_HexEq( const char *a, const char *b )
{
	if ( !a || !b ) {
		return qfalse;
	}
	while ( *a && *b ) {
		char ca = *a;
		char cb = *b;
		if ( ca >= 'A' && ca <= 'F' ) {
			ca = (char)( ca - 'A' + 'a' );
		}
		if ( cb >= 'A' && cb <= 'F' ) {
			cb = (char)( cb - 'A' + 'a' );
		}
		if ( ca != cb ) {
			return qfalse;
		}
		a++;
		b++;
	}
	return *a == *b;
}

qboolean Com_Pk3Sig_VerifyFile( const char *pakPath, const char *sigPath )
{
#if defined( USE_OPENSSL ) || defined( USE_DTLS )
	FILE *pf;
	FILE *sf;
	unsigned char buf[65536];
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digestLen = 0;
	char expect[128];
	char line[256];
	char hex[EVP_MAX_MD_SIZE * 2 + 1];
	EVP_MD_CTX *ctx;
	int i;

	if ( !pakPath || !sigPath ) {
		return qfalse;
	}
	sf = fopen( sigPath, "r" );
	if ( !sf ) {
		return qfalse;
	}
	expect[0] = '\0';
	while ( fgets( line, sizeof( line ), sf ) ) {
		if ( !Q_strncmp( line, "sha256=", 7 ) ) {
			Q_strncpyz( expect, line + 7, sizeof( expect ) );
			break;
		}
	}
	fclose( sf );
	if ( !expect[0] ) {
		return qfalse;
	}
	{
		char *p = expect;
		while ( *p && *p != '\n' && *p != '\r' ) {
			p++;
		}
		*p = '\0';
	}

	pf = fopen( pakPath, "rb" );
	if ( !pf ) {
		return qfalse;
	}
	ctx = EVP_MD_CTX_new();
	if ( !ctx || EVP_DigestInit_ex( ctx, EVP_sha256(), NULL ) != 1 ) {
		if ( ctx ) {
			EVP_MD_CTX_free( ctx );
		}
		fclose( pf );
		return qfalse;
	}
	while ( 1 ) {
		size_t n = fread( buf, 1, sizeof( buf ), pf );
		if ( n == 0 ) {
			break;
		}
		EVP_DigestUpdate( ctx, buf, n );
	}
	fclose( pf );
	EVP_DigestFinal_ex( ctx, digest, &digestLen );
	EVP_MD_CTX_free( ctx );

	for ( i = 0; (unsigned)i < digestLen; i++ ) {
		Com_sprintf( hex + i * 2, 3, "%02x", digest[i] );
	}
	return Com_Pk3Sig_HexEq( hex, expect );
#else
	(void)pakPath;
	(void)sigPath;
	Com_Printf( S_COLOR_YELLOW "WARNING: pk3.sig verification requires OpenSSL build\n" );
	return qtrue;
#endif
}
