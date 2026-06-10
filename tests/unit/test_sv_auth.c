/*
 * Unit tests: server auth token verification (sv_auth.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "server/sv_auth.h"

extern cvar_t *sv_maxclients;

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", (msg)); \
		return 1; \
	} \
} while (0)

static int test_disabled_passes( void )
{
	SV_Auth_Init();
	sv_maxclients = Cvar_Get( "sv_maxclients", "8", 0 );
	Cvar_Set( "sv_authEnable", "0" );
	ASSERT( SV_AuthVerifyToken( NULL, 0 ), "disabled auth accepts all" );
	SV_Auth_Shutdown();
	return 0;
}

static int test_allowlist( void )
{
	SV_Auth_Init();
	Cvar_Set( "sv_authEnable", "1" );
	Cvar_Set( "sv_authRequire", "1" );
	Cvar_Set( "sv_authSecret", "secret" );
	Cvar_Set( "sv_authAllowTokens", "dev-token,other" );
	ASSERT( SV_AuthVerifyToken( "dev-token", 0 ), "allowlist token accepted" );
	ASSERT( !SV_AuthVerifyToken( "missing", 0 ), "non-allowlist rejected with secret set" );
	SV_Auth_Shutdown();
	return 0;
}

static int test_signed_token( void )
{
	long issued;
	char payload[256];
	char token[128];
	char *hash;

	SV_Auth_Init();
	Cvar_Set( "sv_authEnable", "1" );
	Cvar_Set( "sv_authRequire", "1" );
	Cvar_Set( "sv_authSecret", "unit-test-secret" );
	Cvar_Set( "sv_authMaxAge", "3600" );
	Cvar_Set( "sv_authMaxSkew", "120" );
	Cvar_Set( "sv_authAllowTokens", "" );

	issued = (long)time( NULL );
	Com_sprintf( payload, sizeof( payload ), "%s:%ld:%d", "unit-test-secret", issued, 0 );
	hash = Com_MD5Buf( payload, (int)strlen( payload ), NULL, 0 );
	ASSERT( hash != NULL, "MD5 produced hash" );
	Com_sprintf( token, sizeof( token ), "%ld.%s", issued, hash );

	ASSERT( SV_AuthVerifyToken( token, 0 ), "valid signed token accepted" );
	ASSERT( !SV_AuthVerifyToken( token, 1 ), "signed token bound to clientNum" );
	ASSERT( !SV_AuthVerifyToken( "not-a-token", 0 ), "garbage token rejected" );
	SV_Auth_Shutdown();
	return 0;
}

int main( void )
{
	if ( test_disabled_passes() ) {
		return 1;
	}
	if ( test_allowlist() ) {
		return 1;
	}
	if ( test_signed_token() ) {
		return 1;
	}
	printf( "PASS: sv_auth\n" );
	return 0;
}
