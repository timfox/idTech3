/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Client-side emoji rendering implementation.

Emoji are rendered from a color atlas texture (gfx/2d/emoji_atlas)
organized as a 32x32 grid. Each emoji is looked up by Unicode codepoint
via the Q_Emoji_Lookup registry.

Shortcode expansion: ":fire:" in chat text is converted to the UTF-8
sequence for U+1F525 before rendering.
===========================================================================
*/

#include "client.h"
#include "cl_emoji.h"
#include "q_utf8.h"

static qhandle_t    emojiAtlasShader = 0;
static cvar_t       *cl_emoji;
static qboolean     emojiInited = qfalse;

void CL_Emoji_Init( void ) {
	cl_emoji = Cvar_Get( "cl_emoji", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_emoji, "Enable emoji rendering in text (0 = off, 1 = on)." );

	Q_Emoji_Init();

	emojiAtlasShader = 0;
	emojiInited = qtrue;
	Com_Printf( "Emoji rendering: %s (cl_emoji %d)\n",
		cl_emoji->integer ? "enabled" : "disabled", cl_emoji->integer );
}

void CL_Emoji_Shutdown( void ) {
	emojiAtlasShader = 0;
	emojiInited = qfalse;
}

qboolean CL_Emoji_IsEnabled( void ) {
	return emojiInited && cl_emoji && cl_emoji->integer;
}

qboolean CL_Emoji_DrawChar( int x, int y, float w, float h, uint32_t codepoint ) {
	const emojiEntry_t *e;
	float s1, t1, s2, t2;
	float cellSize;

	if ( !CL_Emoji_IsEnabled() ) {
		return qfalse;
	}

	e = Q_Emoji_Lookup( codepoint );
	if ( !e ) {
		return qfalse;
	}

	if ( !emojiAtlasShader && re.RegisterShaderNoMip ) {
		emojiAtlasShader = re.RegisterShaderNoMip( "gfx/2d/emoji_atlas" );
	}

	if ( !emojiAtlasShader ) {
		float ax = (float)x, ay = (float)y, aw = w, ah = h;
		SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
		re.SetColor( NULL );
		re.DrawStretchPic( ax, ay, aw, ah, 0, 0, 1, 1, cls.whiteShader );
		return qtrue;
	}

	cellSize = 1.0f / (float)Q_EMOJI_ATLAS_SIZE;
	s1 = (float)e->atlasX * cellSize;
	t1 = (float)e->atlasY * cellSize;
	s2 = s1 + cellSize;
	t2 = t1 + cellSize;

	{
		float ax = (float)x, ay = (float)y, aw = w, ah = h;
		SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
		re.SetColor( NULL );
		re.DrawStretchPic( ax, ay, aw, ah, s1, t1, s2, t2, emojiAtlasShader );
	}

	return qtrue;
}

int CL_Emoji_ExpandShortcodes( const char *in, char *out, int outSize ) {
	const char *p = in;
	int written = 0;

	if ( !CL_Emoji_IsEnabled() ) {
		Q_strncpyz( out, in, outSize );
		return (int)strlen( out );
	}

	while ( *p && written < outSize - 5 ) {
		if ( *p == ':' ) {
			const char *end = strchr( p + 1, ':' );
			if ( end && ( end - p ) > 1 && ( end - p ) < 32 ) {
				char shortcode[32];
				int len = (int)( end - p - 1 );
				const emojiEntry_t *e;

				Q_strncpyz( shortcode, p + 1, len + 1 );
				e = Q_Emoji_FindByShortcode( shortcode );
				if ( e ) {
					char utf8[5];
					int nbytes = Q_UTF8_Encode( e->codepoint, utf8 );
					if ( written + nbytes < outSize - 1 ) {
						Com_Memcpy( out + written, utf8, nbytes );
						written += nbytes;
						p = end + 1;
						continue;
					}
				}
			}
		}
		out[written++] = *p++;
	}
	out[written] = '\0';
	return written;
}
