/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Account token verification for dedicated servers (userinfo authToken).
Signed format: <unix_time>.<32_hex_md5> where md5 = MD5( secret:time:clientNum ).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "sv_auth.h"

#include <string.h>
#include <time.h>

extern cvar_t *sv_maxclients;

static cvar_t *sv_authEnable;
static cvar_t *sv_authUserinfoKey;
static cvar_t *sv_authSecret;
static cvar_t *sv_authMaxAge;
static cvar_t *sv_authMaxSkew;
static cvar_t *sv_authAllowTokens;
static cvar_t *sv_authRequire;

static qboolean SV_AuthTokenInAllowlist( const char *token )
{
	const char *list;
	const char *start;
	const char *end;
	char entry[256];
	size_t len;

	if ( !token || !token[0] || !sv_authAllowTokens || !sv_authAllowTokens->string[0] ) {
		return qfalse;
	}

	list = sv_authAllowTokens->string;
	while ( *list ) {
		while ( *list == ' ' || *list == '\t' ) {
			list++;
		}
		if ( !*list ) {
			break;
		}
		start = list;
		while ( *list && *list != ',' ) {
			list++;
		}
		end = list;
		while ( end > start && ( end[-1] == ' ' || end[-1] == '\t' ) ) {
			end--;
		}
		len = (size_t)( end - start );
		if ( len >= sizeof( entry ) ) {
			len = sizeof( entry ) - 1;
		}
		memcpy( entry, start, len );
		entry[len] = '\0';
		if ( entry[0] && !Q_stricmp( entry, token ) ) {
			return qtrue;
		}
		if ( *list == ',' ) {
			list++;
		}
	}
	return qfalse;
}

static qboolean SV_AuthValidateSigned( const char *token, int clientNum )
{
	const char *secret;
	char payload[256];
	char tokenCopy[512];
	char *dot;
	char *md5hex;
	char *end;
	long issued;
	long now;
	long age;
	int i;

	if ( !sv_authSecret || !sv_authSecret->string[0] ) {
		Com_Printf( S_COLOR_YELLOW "[auth] sv_authSecret unset — signed tokens rejected\n" );
		return qfalse;
	}

	secret = sv_authSecret->string;
	Q_strncpyz( tokenCopy, token, sizeof( tokenCopy ) );
	dot = strchr( tokenCopy, '.' );
	if ( !dot || dot == tokenCopy ) {
		return qfalse;
	}

	issued = strtol( tokenCopy, &end, 10 );
	if ( end != dot || issued <= 0 ) {
		return qfalse;
	}

	md5hex = dot + 1;
	if ( strlen( md5hex ) != 32 ) {
		return qfalse;
	}
	for ( i = 0; i < 32; i++ ) {
		char c = md5hex[i];
		if ( ! ( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' ) ) ) {
			return qfalse;
		}
	}

	now = (long)time( NULL );
	age = now - issued;
	if ( age > sv_authMaxAge->integer ) {
		return qfalse;
	}
	if ( age < -sv_authMaxSkew->integer ) {
		return qfalse;
	}

	Com_sprintf( payload, sizeof( payload ), "%s:%ld:%d", secret, issued, clientNum );
	md5hex = Com_MD5Buf( payload, (int)strlen( payload ), NULL, 0 );
	if ( !md5hex ) {
		return qfalse;
	}

	return !Q_stricmp( md5hex, dot + 1 );
}

void SV_Auth_Init( void )
{
	sv_authEnable = Cvar_Get( "sv_authEnable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_authEnable,
		"When 1, clients must present a valid auth token in userinfo (see sv_authUserinfoKey)." );

	sv_authUserinfoKey = Cvar_Get( "sv_authUserinfoKey", "authToken", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_authUserinfoKey,
		"Userinfo key carrying the account auth token." );

	sv_authSecret = Cvar_Get( "sv_authSecret", "", CVAR_ARCHIVE | CVAR_PROTECTED );
	Cvar_SetDescription( sv_authSecret,
		"Shared secret for signed tokens: <unix_time>.<md5(secret:time:clientNum)>." );

	sv_authMaxAge = Cvar_Get( "sv_authMaxAge", "3600", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_authMaxAge, "60", "604800", CV_INTEGER );
	Cvar_SetDescription( sv_authMaxAge,
		"Reject signed tokens older than this many seconds." );

	sv_authMaxSkew = Cvar_Get( "sv_authMaxSkew", "120", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_authMaxSkew, "0", "3600", CV_INTEGER );
	Cvar_SetDescription( sv_authMaxSkew,
		"Allow signed token timestamps up to this many seconds in the future (clock skew)." );

	sv_authAllowTokens = Cvar_Get( "sv_authAllowTokens", "", CVAR_ARCHIVE | CVAR_PROTECTED );
	Cvar_SetDescription( sv_authAllowTokens,
		"Optional comma-separated static tokens that always pass (dev / service accounts)." );

	sv_authRequire = Cvar_Get( "sv_authRequire", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( sv_authRequire, "0", "1", CV_INTEGER );
	Cvar_SetDescription( sv_authRequire,
		"When sv_authEnable 1, reject connects with missing/invalid tokens (0 = log only on userinfo updates)." );

	if ( sv_authEnable->integer ) {
		Com_Printf( "[auth] enabled (key '%s', maxAge %ds, require %s)\n",
			sv_authUserinfoKey->string,
			sv_authMaxAge->integer,
			sv_authRequire->integer ? "on" : "off" );
	}

	Cmd_AddCommand( "auth_maketoken", SV_Auth_MakeToken_f );
}

void SV_Auth_Shutdown( void )
{
	Cmd_RemoveCommand( "auth_maketoken" );
}

qboolean SV_AuthVerifyToken( const char *token, int clientNum )
{
	if ( !sv_authEnable || !sv_authEnable->integer ) {
		return qtrue;
	}

	if ( !token || !token[0] ) {
		return sv_authRequire->integer ? qfalse : qtrue;
	}

	if ( SV_AuthTokenInAllowlist( token ) ) {
		return qtrue;
	}

	if ( strchr( token, '.' ) ) {
		return SV_AuthValidateSigned( token, clientNum );
	}

	/* Legacy: non-empty opaque token when no secret configured. */
	if ( !sv_authSecret || !sv_authSecret->string[0] ) {
		return qtrue;
	}

	return qfalse;
}

const char *SV_Auth_UserinfoKey( void )
{
	if ( sv_authUserinfoKey && sv_authUserinfoKey->string[0] ) {
		return sv_authUserinfoKey->string;
	}
	return "authToken";
}

void SV_Auth_MakeToken_f( void )
{
	long issued;
	char payload[256];
	char *hash;
	int clientNum;

	if ( !sv_authSecret || !sv_authSecret->string[0] ) {
		Com_Printf( "Set sv_authSecret before generating tokens.\n" );
		return;
	}

	clientNum = 0;
	if ( Cmd_Argc() >= 2 ) {
		clientNum = atoi( Cmd_Argv( 1 ) );
	}
	if ( clientNum < 0 || clientNum >= ( sv_maxclients ? sv_maxclients->integer : 0 ) ) {
		Com_Printf( "Usage: auth_maketoken [clientNum 0..%d]\n",
			( sv_maxclients ? sv_maxclients->integer : 1 ) - 1 );
		return;
	}

	issued = (long)time( NULL );
	Com_sprintf( payload, sizeof( payload ), "%s:%ld:%d",
		sv_authSecret->string, issued, clientNum );
	hash = Com_MD5Buf( payload, (int)strlen( payload ), NULL, 0 );
	if ( !hash ) {
		Com_Printf( S_COLOR_RED "[auth] MD5 failed\n" );
		return;
	}

	Com_Printf( "authToken \"%ld.%s\"  (clientNum %d, valid ~%ds)\n",
		issued, hash, clientNum, sv_authMaxAge->integer );
}
