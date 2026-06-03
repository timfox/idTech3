/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side vector font text drawing (Lengyel JCGT 2017 GPU outline path).
===========================================================================
*/

#include "client.h"
#include "cl_vector_font.h"

static cvar_t *r_vectorFont;
static qboolean vectorLoadAttempted;

void VectorFont_Init( void ) {
	r_vectorFont = Cvar_Get( "r_vectorFont", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_vectorFont,
		"Draw console/HUD text from TrueType outlines on the GPU (Lengyel 2017). "
		"Requires Vulkan and r_font .ttf. 1 = vector outlines; 0 = FreeType atlas (default)." );
	vectorLoadAttempted = qfalse;
}

static qboolean VectorFont_EnsureLoaded( void ) {
	const char *fontPath;

	if ( !r_vectorFont || !r_vectorFont->integer ) {
		return qfalse;
	}
	if ( !re.LoadVectorFont || !re.VectorFontActive || !re.DrawVectorString ) {
		return qfalse;
	}
	if ( re.VectorFontActive() ) {
		return qtrue;
	}
	if ( vectorLoadAttempted ) {
		return qfalse;
	}
	vectorLoadAttempted = qtrue;

	fontPath = Cvar_VariableString( "r_font" );
	if ( !fontPath || !fontPath[0] ) {
		return qfalse;
	}
	if ( !re.LoadVectorFont( fontPath ) ) {
		Com_Printf( S_COLOR_YELLOW "VectorFont: failed to load '%s'\n", fontPath );
		return qfalse;
	}
	Com_Printf( "VectorFont: GPU outline text enabled for '%s'\n", fontPath );
	return qtrue;
}

void VectorFont_Reload( void ) {
	vectorLoadAttempted = qfalse;
	if ( re.LoadVectorFont && r_vectorFont && r_vectorFont->integer ) {
		const char *fontPath = Cvar_VariableString( "r_font" );
		if ( fontPath && fontPath[0] ) {
			re.LoadVectorFont( fontPath );
		}
	}
}

qboolean VectorFont_IsActive( void ) {
	return VectorFont_EnsureLoaded();
}

qboolean VectorFont_DrawStringExt( int x, int y, float size, const char *string,
	const float *setColor, qboolean forceColor, qboolean noColorEscape,
	qboolean virtual640 ) {
	float scale;
	float shadowOff;
	vec4_t color;

	(void)forceColor;
	(void)noColorEscape;
	(void)virtual640;

	if ( !VectorFont_EnsureLoaded() || !string || !setColor || !re.DrawVectorString ) {
		return qfalse;
	}
	if ( !string[0] ) {
		return qfalse;
	}

	scale = size / 48.0f;
	shadowOff = (float)Cvar_VariableIntegerValue( "r_fontShadow" );

	Com_Memcpy( color, setColor, sizeof( color ) );
	return re.DrawVectorString( (float)x, (float)y, scale, string, color, shadowOff );
}
