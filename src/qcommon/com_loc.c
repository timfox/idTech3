/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "com_loc.h"

#define MAX_LOC_ENTRIES 4096

typedef struct {
	char key[64];
	char value[256];
} locEntry_t;

static locEntry_t s_loc[MAX_LOC_ENTRIES];
static int s_locCount;
static cvar_t *com_loc_language;

void Com_Loc_Clear( void ) {
	s_locCount = 0;
	Com_Memset( s_loc, 0, sizeof( s_loc ) );
}

static void Com_Loc_ParseBuffer( const char *buf ) {
	const char *p;
	char key[64];
	char value[256];

	if ( !buf ) {
		return;
	}

	p = buf;
	while ( *p ) {
		const char *lineEnd = strchr( p, '\n' );
		size_t lineLen;
		char line[512];
		char *eq;

		if ( !lineEnd ) {
			lineEnd = p + strlen( p );
		}
		lineLen = (size_t)( lineEnd - p );
		if ( lineLen >= sizeof( line ) ) {
			lineLen = sizeof( line ) - 1;
		}
		Com_Memcpy( line, p, lineLen );
		line[lineLen] = '\0';
		p = ( *lineEnd ) ? lineEnd + 1 : lineEnd;

		while ( line[0] == ' ' || line[0] == '\t' || line[0] == '\r' ) {
			memmove( line, line + 1, strlen( line ) );
		}
		if ( !line[0] || line[0] == '#' || line[0] == ';' ) {
			continue;
		}

		eq = strchr( line, '=' );
		if ( !eq ) {
			continue;
		}
		*eq = '\0';
		Q_strncpyz( key, line, sizeof( key ) );
		Q_strncpyz( value, eq + 1, sizeof( value ) );

		if ( s_locCount < MAX_LOC_ENTRIES ) {
			Q_strncpyz( s_loc[s_locCount].key, key, sizeof( s_loc[s_locCount].key ) );
			Q_strncpyz( s_loc[s_locCount].value, value, sizeof( s_loc[s_locCount].value ) );
			s_locCount++;
		}
	}
}

void Com_Loc_Reload( void ) {
	char path[MAX_QPATH];
	void *buf;
	int len;

	Com_Loc_Clear();

	if ( !com_loc_language ) {
		return;
	}

	Com_sprintf( path, sizeof( path ), "loc/%s.loc", com_loc_language->string );
	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( "[loc] no table: %s\n", path );
		return;
	}

	Com_Loc_ParseBuffer( (const char *)buf );
	FS_FreeFile( buf );
	Com_Printf( "[loc] loaded %d strings from %s\n", s_locCount, path );
}

void Com_Loc_Init( void ) {
	com_loc_language = Cvar_Get( "com_loc_language", "en", CVAR_ARCHIVE );
	Cvar_SetDescription( com_loc_language, "Language code for loc/<lang>.loc string tables." );
	Com_Loc_Reload();
}

int Com_Loc_Lookup( const char *key, char *out, int outSize ) {
	int i;

	if ( !key || !key[0] || !out || outSize <= 0 ) {
		return 0;
	}

	for ( i = 0; i < s_locCount; i++ ) {
		if ( !Q_stricmp( s_loc[i].key, key ) ) {
			Q_strncpyz( out, s_loc[i].value, outSize );
			return (int)strlen( out );
		}
	}

	Q_strncpyz( out, key, outSize );
	return (int)strlen( out );
}
