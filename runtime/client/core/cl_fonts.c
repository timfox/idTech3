/*
===========================================================================
cl_fonts.c - discover fonts TTF faces for mods (Quake3e RegisterFontAtlas)
===========================================================================
*/

#include "client.h"

#include <stdlib.h>

#define CL_FONT_MAX           16
#define CL_FONT_LISTBUF_SIZE  4096

static char cl_fontNames[CL_FONT_MAX][MAX_QPATH];
static int  cl_fontCount;
static char cl_fontFallback[MAX_QPATH];

/*
================
CL_FontSortCmp
================
*/
static int QDECL CL_FontSortCmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, (const char *)b );
}

/*
================
CL_Fonts_f
================
*/
static void CL_Fonts_f( void ) {
	int i;

	if ( cl_fontCount <= 0 ) {
		Com_Printf( "No fonts/*.ttf found.\n" );
		return;
	}
	Com_Printf( "Available fonts (%i):\n", cl_fontCount );
	for ( i = 0; i < cl_fontCount; i++ ) {
		Com_Printf( "  %s\n", cl_fontNames[i] );
	}
	if ( cl_fontFallback[0] ) {
		Com_Printf( "Fallback: %s\n", cl_fontFallback );
	}
}

/*
================
CL_InitFonts
================
*/
void CL_InitFonts( void ) {
	char listbuf[CL_FONT_LISTBUF_SIZE];
	char sorted[CL_FONT_MAX][MAX_QPATH];
	char listStr[MAX_CVAR_VALUE_STRING];
	const char *p;
	int nFiles;
	int i;
	int count;
	int pos;
	int nameLen;
	int need;
	cvar_t *cv;

	cl_fontCount = 0;
	cl_fontFallback[0] = '\0';
	Com_Memset( cl_fontNames, 0, sizeof( cl_fontNames ) );
	Com_Memset( sorted, 0, sizeof( sorted ) );

	nFiles = FS_GetFileList( "fonts", "ttf", listbuf, sizeof( listbuf ) );
	p = listbuf;
	count = 0;

	for ( i = 0; i < nFiles && count < CL_FONT_MAX; i++ ) {
		if ( !p || !p[0] ) {
			break;
		}
		if ( strchr( p, '/' ) || strchr( p, '\\' ) ) {
			p += strlen( p ) + 1;
			continue;
		}
		Q_strncpyz( sorted[count], p, sizeof( sorted[count] ) );
		count++;
		p += strlen( p ) + 1;
	}

	if ( count > 1 ) {
		qsort( sorted, count, sizeof( sorted[0] ), CL_FontSortCmp );
	}

	cl_fontCount = count;
	for ( i = 0; i < cl_fontCount; i++ ) {
		Q_strncpyz( cl_fontNames[i], sorted[i], sizeof( cl_fontNames[i] ) );
	}

	if ( cl_fontCount > 0 ) {
		Com_sprintf( cl_fontFallback, sizeof( cl_fontFallback ), "fonts/%s", cl_fontNames[0] );
	}

	listStr[0] = '\0';
	pos = 0;
	for ( i = 0; i < cl_fontCount; i++ ) {
		nameLen = (int)strlen( cl_fontNames[i] );
		if ( nameLen <= 0 ) {
			continue;
		}
		need = nameLen + ( pos > 0 ? 1 : 0 );
		if ( pos + need + 1 > (int)sizeof( listStr ) ) {
			break;
		}
		if ( pos > 0 ) {
			listStr[pos++] = ',';
		}
		Com_Memcpy( listStr + pos, cl_fontNames[i], nameLen );
		pos += nameLen;
		listStr[pos] = '\0';
	}

	cv = Cvar_Get( "cl_fontList", "",
		CVAR_ROM | CVAR_PROTECTED | CVAR_TEMP
#ifdef CVAR_NOTABCOMPLETE
		| CVAR_NOTABCOMPLETE
#endif
		);
	Cvar_SetDescription( cv, "Comma-separated fonts/*.ttf basenames for mods (max 16)." );
	Cvar_Set( "cl_fontList", listStr );

	Cmd_RemoveCommand( "fonts" );
	Cmd_AddCommand( "fonts", CL_Fonts_f );

	if ( cl_fontCount > 0 ) {
		Com_Printf( "Fonts: %i face(s) in fonts/*.ttf (cl_fontList)\n", cl_fontCount );
	}
}

/*
================
CL_FontResolvedPath
================
*/
const char *CL_FontResolvedPath( void ) {
	if ( !cl_fontFallback[0] ) {
		return NULL;
	}
	return cl_fontFallback;
}
