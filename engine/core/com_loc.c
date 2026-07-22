/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Localization tables: loc/<lang>.loc key=value with escapes, multiline
continuation (\ at EOL), duplicate diagnostics, and language fallback.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "com_loc.h"

typedef struct {
	char key[COM_LOC_KEY_SIZE];
	char value[COM_LOC_VALUE_SIZE];
	unsigned hash;
} locEntry_t;

static locEntry_t s_loc[COM_LOC_MAX_ENTRIES];
static int s_locCount;
static cvar_t *com_loc_language;
static char s_loadedPath[MAX_QPATH];

static unsigned Com_Loc_Hash( const char *s ) {
	unsigned h = 5381;
	while ( *s ) {
		h = ( ( h << 5 ) + h ) + (unsigned char)( *s++ );
	}
	return h;
}

static qboolean Com_Loc_Utf8Ok( const char *s ) {
	const unsigned char *p = (const unsigned char *)s;
	while ( *p ) {
		if ( *p < 0x80 ) {
			p++;
			continue;
		}
		if ( ( p[0] & 0xE0 ) == 0xC0 ) {
			if ( ( p[1] & 0xC0 ) != 0x80 ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( ( p[0] & 0xF0 ) == 0xE0 ) {
			if ( ( p[1] & 0xC0 ) != 0x80 || ( p[2] & 0xC0 ) != 0x80 ) {
				return qfalse;
			}
			p += 3;
			continue;
		}
		if ( ( p[0] & 0xF8 ) == 0xF0 ) {
			if ( ( p[1] & 0xC0 ) != 0x80 || ( p[2] & 0xC0 ) != 0x80 || ( p[3] & 0xC0 ) != 0x80 ) {
				return qfalse;
			}
			p += 4;
			continue;
		}
		return qfalse;
	}
	return qtrue;
}

static void Com_Loc_Unescape( char *dst, int dstSize, const char *src ) {
	int o = 0;
	while ( *src && o < dstSize - 1 ) {
		if ( *src == '\\' && src[1] ) {
			src++;
			switch ( *src ) {
			case 'n': dst[o++] = '\n'; break;
			case 't': dst[o++] = '\t'; break;
			case 'r': dst[o++] = '\r'; break;
			case '\\': dst[o++] = '\\'; break;
			case '=': dst[o++] = '='; break;
			default: dst[o++] = *src; break;
			}
			src++;
			continue;
		}
		dst[o++] = *src++;
	}
	dst[o] = '\0';
}

void Com_Loc_Clear( void ) {
	s_locCount = 0;
	Com_Memset( s_loc, 0, sizeof( s_loc ) );
	s_loadedPath[0] = '\0';
}

static void Com_Loc_AddEntry( const char *key, const char *value ) {
	int i;
	char unesc[COM_LOC_VALUE_SIZE];

	if ( !key || !key[0] || !value ) {
		return;
	}
	if ( !Com_Loc_Utf8Ok( key ) || !Com_Loc_Utf8Ok( value ) ) {
		Com_Printf( S_COLOR_YELLOW "[loc] skip non-UTF-8 key '%s'\n", key );
		return;
	}

	Com_Loc_Unescape( unesc, sizeof( unesc ), value );

	for ( i = 0; i < s_locCount; i++ ) {
		if ( !Q_stricmp( s_loc[i].key, key ) ) {
			Com_Printf( S_COLOR_YELLOW "[loc] duplicate key '%s' (overwriting)\n", key );
			Q_strncpyz( s_loc[i].value, unesc, sizeof( s_loc[i].value ) );
			s_loc[i].hash = Com_Loc_Hash( s_loc[i].key );
			return;
		}
	}

	if ( s_locCount >= COM_LOC_MAX_ENTRIES ) {
		Com_Printf( S_COLOR_YELLOW "[loc] table full (%d), dropping '%s'\n", COM_LOC_MAX_ENTRIES, key );
		return;
	}

	Q_strncpyz( s_loc[s_locCount].key, key, sizeof( s_loc[0].key ) );
	Q_strncpyz( s_loc[s_locCount].value, unesc, sizeof( s_loc[0].value ) );
	s_loc[s_locCount].hash = Com_Loc_Hash( s_loc[s_locCount].key );
	s_locCount++;
}

static void Com_Loc_ParseBuffer( const char *buf ) {
	const char *p;
	char accum[COM_LOC_VALUE_SIZE * 2];
	int accumLen = 0;
	qboolean inCont = qfalse;
	char pendingKey[COM_LOC_KEY_SIZE];

	if ( !buf ) {
		return;
	}

	pendingKey[0] = '\0';
	p = buf;
	while ( *p || inCont ) {
		const char *lineEnd;
		size_t lineLen;
		char line[COM_LOC_VALUE_SIZE + 64];
		char *eq;
		int softCont = 0;

		if ( !*p && !inCont ) {
			break;
		}

		lineEnd = strchr( p, '\n' );
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
		{
			size_t L = strlen( line );
			while ( L > 0 && ( line[L - 1] == '\r' || line[L - 1] == ' ' || line[L - 1] == '\t' ) ) {
				line[--L] = '\0';
			}
			if ( L > 0 && line[L - 1] == '\\' ) {
				line[L - 1] = '\0';
				softCont = 1;
			}
		}

		if ( !inCont ) {
			if ( !line[0] || line[0] == '#' || line[0] == ';' ) {
				continue;
			}
			eq = strchr( line, '=' );
			if ( !eq ) {
				continue;
			}
			*eq = '\0';
			Q_strncpyz( pendingKey, line, sizeof( pendingKey ) );
			Q_strncpyz( accum, eq + 1, sizeof( accum ) );
			accumLen = (int)strlen( accum );
			if ( softCont ) {
				inCont = qtrue;
				continue;
			}
			Com_Loc_AddEntry( pendingKey, accum );
			pendingKey[0] = '\0';
		} else {
			if ( accumLen + 1 < (int)sizeof( accum ) ) {
				accum[accumLen++] = '\n';
				accum[accumLen] = '\0';
			}
			Q_strcat( accum, sizeof( accum ), line );
			accumLen = (int)strlen( accum );
			if ( softCont ) {
				continue;
			}
			Com_Loc_AddEntry( pendingKey, accum );
			pendingKey[0] = '\0';
			inCont = qfalse;
			accumLen = 0;
		}
	}
}

static qboolean Com_Loc_LoadFile( const char *lang ) {
	char path[MAX_QPATH];
	void *buf;
	int len;

	Com_sprintf( path, sizeof( path ), "loc/%s.loc", lang );
	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		return qfalse;
	}
	Com_Loc_ParseBuffer( (const char *)buf );
	FS_FreeFile( buf );
	Q_strncpyz( s_loadedPath, path, sizeof( s_loadedPath ) );
	Com_Printf( "[loc] loaded %d strings from %s\n", s_locCount, path );
	return qtrue;
}

void Com_Loc_Reload( void ) {
	char lang[32];
	char base[32];
	char *dash;

	Com_Loc_Clear();

	if ( !com_loc_language ) {
		return;
	}

	Q_strncpyz( lang, com_loc_language->string, sizeof( lang ) );
	if ( !lang[0] ) {
		Q_strncpyz( lang, "en", sizeof( lang ) );
	}

	/* language-region -> language -> en */
	if ( !Com_Loc_LoadFile( lang ) ) {
		Q_strncpyz( base, lang, sizeof( base ) );
		dash = strchr( base, '-' );
		if ( !dash ) {
			dash = strchr( base, '_' );
		}
		if ( dash ) {
			*dash = '\0';
			if ( Com_Loc_LoadFile( base ) ) {
				return;
			}
		}
		if ( Q_stricmp( lang, "en" ) && Q_stricmp( base, "en" ) ) {
			if ( !Com_Loc_LoadFile( "en" ) ) {
				Com_Printf( "[loc] no table: loc/%s.loc (also tried fallbacks)\n", lang );
			}
		} else {
			Com_Printf( "[loc] no table: loc/%s.loc\n", lang );
		}
	}
}

static void Com_Loc_Reload_f( void ) {
	Com_Loc_Reload();
}

static void Com_Loc_Status_f( void ) {
	Com_Loc_Status();
}

void Com_Loc_Status( void ) {
	Com_Printf( "[loc] language=%s entries=%d path=%s valueCap=%d\n",
		com_loc_language ? com_loc_language->string : "?",
		s_locCount,
		s_loadedPath[0] ? s_loadedPath : "(none)",
		COM_LOC_VALUE_SIZE );
}

void Com_Loc_Init( void ) {
	com_loc_language = Cvar_Get( "com_loc_language", "en", CVAR_ARCHIVE );
	Cvar_SetDescription( com_loc_language, "Language code for loc/<lang>.loc string tables (fallback: region strip, then en)." );
	Cmd_AddCommand( "loc_reload", Com_Loc_Reload_f );
	Cmd_AddCommand( "loc_status", Com_Loc_Status_f );
	Com_Loc_Reload();
	Com_Printf( "Com_Loc: ready (com_loc_language=%s, valueCap=%d)\n",
		com_loc_language->string, COM_LOC_VALUE_SIZE );
}

int Com_Loc_Count( void ) {
	return s_locCount;
}

const char *Com_Loc_Language( void ) {
	return com_loc_language ? com_loc_language->string : "en";
}

int Com_Loc_Lookup( const char *key, char *out, int outSize ) {
	int i;
	unsigned h;

	if ( !key || !key[0] || !out || outSize <= 0 ) {
		return 0;
	}

	h = Com_Loc_Hash( key );
	for ( i = 0; i < s_locCount; i++ ) {
		if ( s_loc[i].hash == h && !Q_stricmp( s_loc[i].key, key ) ) {
			Q_strncpyz( out, s_loc[i].value, outSize );
			return (int)strlen( out );
		}
	}

	Q_strncpyz( out, key, outSize );
	return (int)strlen( out );
}
