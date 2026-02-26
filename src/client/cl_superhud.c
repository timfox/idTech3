/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SuperHUD configurable HUD layout system.

Loads HUD configurations from .cfg files with a declarative format:
  !element StatusBar_HealthCount {
    rect 4 464 40 16
    color 1 1 1 1
    bgcolor 0 0 0 0.5
    font "fonts/default" 16
    text "%H"
  }

Elements are positioned using a 640x480 virtual coordinate system
and scaled to the actual screen resolution at render time.
===========================================================================
*/

#include "client.h"
#include "cl_superhud.h"

static shudConfig_t activeConfig;
static cvar_t *cg_shud;
static cvar_t *cg_shud_file;
static qboolean shudInitialized = qfalse;

void SHUD_Init( void ) {
	cg_shud = Cvar_Get( "cg_shud", "0", CVAR_ARCHIVE );
	cg_shud_file = Cvar_Get( "cg_shud_file", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cg_shud, "Enable SuperHUD configurable HUD (0 = default HUD, 1 = SuperHUD)." );
	Cvar_SetDescription( cg_shud_file, "SuperHUD configuration file to load (e.g. hud/myhud.cfg)." );

	Com_Memset( &activeConfig, 0, sizeof( activeConfig ) );
	shudInitialized = qtrue;

	if ( cg_shud->integer && cg_shud_file->string[0] ) {
		SHUD_LoadConfig( cg_shud_file->string );
	}

	Com_Printf( "SuperHUD: %s (cg_shud %d)\n",
		cg_shud->integer ? "enabled" : "disabled", cg_shud->integer );
}

void SHUD_Shutdown( void ) {
	Com_Memset( &activeConfig, 0, sizeof( activeConfig ) );
	shudInitialized = qfalse;
}

static qboolean SHUD_ParseElement( const char **pp, shudElement_t *elem ) {
	const char *token;
	Com_Memset( elem, 0, sizeof( *elem ) );
	elem->active = qtrue;
	elem->color[0] = elem->color[1] = elem->color[2] = elem->color[3] = 1.0f;
	elem->fontsize = 16;

	token = COM_Parse( pp );
	if ( !token[0] ) return qfalse;
	Q_strncpyz( elem->name, token, sizeof( elem->name ) );

	token = COM_Parse( pp );
	if ( Q_stricmp( token, "{" ) != 0 ) return qfalse;

	while ( 1 ) {
		token = COM_Parse( pp );
		if ( !token[0] || token[0] == '}' ) break;

		if ( Q_stricmp( token, "rect" ) == 0 ) {
			elem->x = atof( COM_Parse( pp ) );
			elem->y = atof( COM_Parse( pp ) );
			elem->w = atof( COM_Parse( pp ) );
			elem->h = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "color" ) == 0 ) {
			elem->color[0] = atof( COM_Parse( pp ) );
			elem->color[1] = atof( COM_Parse( pp ) );
			elem->color[2] = atof( COM_Parse( pp ) );
			elem->color[3] = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "bgcolor" ) == 0 ) {
			elem->bgcolor[0] = atof( COM_Parse( pp ) );
			elem->bgcolor[1] = atof( COM_Parse( pp ) );
			elem->bgcolor[2] = atof( COM_Parse( pp ) );
			elem->bgcolor[3] = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "bordercolor" ) == 0 ) {
			elem->bordercolor[0] = atof( COM_Parse( pp ) );
			elem->bordercolor[1] = atof( COM_Parse( pp ) );
			elem->bordercolor[2] = atof( COM_Parse( pp ) );
			elem->bordercolor[3] = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "border" ) == 0 ) {
			elem->border = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "text" ) == 0 ) {
			Q_strncpyz( elem->text, COM_Parse( pp ), sizeof( elem->text ) );
		} else if ( Q_stricmp( token, "font" ) == 0 ) {
			Q_strncpyz( elem->font, COM_Parse( pp ), sizeof( elem->font ) );
			elem->fontsize = atof( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "style" ) == 0 ) {
			elem->style = atoi( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "visflags" ) == 0 ) {
			elem->visflags = atoi( COM_Parse( pp ) );
		} else if ( Q_stricmp( token, "type" ) == 0 ) {
			token = COM_Parse( pp );
			if ( Q_stricmp( token, "text" ) == 0 ) elem->type = SHUD_TEXT;
			else if ( Q_stricmp( token, "bar" ) == 0 ) elem->type = SHUD_BAR;
			else if ( Q_stricmp( token, "icon" ) == 0 ) elem->type = SHUD_ICON;
			else if ( Q_stricmp( token, "number" ) == 0 ) elem->type = SHUD_NUMBER;
			else if ( Q_stricmp( token, "timer" ) == 0 ) elem->type = SHUD_TIMER;
			else if ( Q_stricmp( token, "rect" ) == 0 ) elem->type = SHUD_RECT;
		}
	}
	return qtrue;
}

void SHUD_LoadConfig( const char *filename ) {
	void *buf;
	int len;
	const char *p;
	const char *token;

	if ( !filename || !filename[0] ) return;

	len = FS_ReadFile( filename, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "SuperHUD: could not load %s\n", filename );
		return;
	}

	Com_Memset( &activeConfig, 0, sizeof( activeConfig ) );
	Q_strncpyz( activeConfig.name, filename, sizeof( activeConfig.name ) );

	p = (const char *)buf;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) break;

		if ( token[0] == '!' || Q_stricmp( token, "element" ) == 0 ) {
			if ( activeConfig.numElements < SHUD_MAX_ELEMENTS ) {
				if ( SHUD_ParseElement( &p, &activeConfig.elements[activeConfig.numElements] ) ) {
					activeConfig.numElements++;
				}
			}
		}
	}

	activeConfig.loaded = qtrue;
	FS_FreeFile( buf );

	Com_Printf( "SuperHUD: loaded %s (%d elements)\n", filename, activeConfig.numElements );
}

int SHUD_AddElement( const char *name, shudElementType_t type ) {
	if ( activeConfig.numElements >= SHUD_MAX_ELEMENTS ) return -1;
	int idx = activeConfig.numElements++;
	shudElement_t *e = &activeConfig.elements[idx];
	Com_Memset( e, 0, sizeof( *e ) );
	Q_strncpyz( e->name, name, sizeof( e->name ) );
	e->type = type;
	e->active = qtrue;
	e->color[0] = e->color[1] = e->color[2] = e->color[3] = 1.0f;
	return idx;
}

void SHUD_SetElementPos( int idx, float x, float y, float w, float h ) {
	if ( idx < 0 || idx >= activeConfig.numElements ) return;
	activeConfig.elements[idx].x = x;
	activeConfig.elements[idx].y = y;
	activeConfig.elements[idx].w = w;
	activeConfig.elements[idx].h = h;
}

void SHUD_SetElementColor( int idx, const vec4_t color ) {
	if ( idx < 0 || idx >= activeConfig.numElements ) return;
	Vector4Copy( color, activeConfig.elements[idx].color );
}

void SHUD_SetElementText( int idx, const char *text ) {
	if ( idx < 0 || idx >= activeConfig.numElements ) return;
	Q_strncpyz( activeConfig.elements[idx].text, text, sizeof( activeConfig.elements[idx].text ) );
}

void SHUD_Render( int screenW, int screenH ) {
	int i;
	float scaleX, scaleY;

	if ( !shudInitialized || !cg_shud || !cg_shud->integer ) return;
	if ( !activeConfig.loaded || activeConfig.numElements == 0 ) return;

	scaleX = (float)screenW / 640.0f;
	scaleY = (float)screenH / 480.0f;

	for ( i = 0; i < activeConfig.numElements; i++ ) {
		shudElement_t *e = &activeConfig.elements[i];
		if ( !e->active ) continue;

		float rx = e->x * scaleX;
		float ry = e->y * scaleY;
		float rw = e->w * scaleX;
		float rh = e->h * scaleY;

		if ( e->bgcolor[3] > 0.0f ) {
			re.SetColor( e->bgcolor );
			re.DrawStretchPic( rx, ry, rw, rh, 0, 0, 1, 1, cls.whiteShader );
		}

		if ( e->border > 0.0f && e->bordercolor[3] > 0.0f ) {
			float b = e->border * scaleX;
			re.SetColor( e->bordercolor );
			re.DrawStretchPic( rx, ry, rw, b, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( rx, ry + rh - b, rw, b, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( rx, ry + b, b, rh - 2*b, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( rx + rw - b, ry + b, b, rh - 2*b, 0, 0, 1, 1, cls.whiteShader );
		}

		if ( e->type == SHUD_TEXT && e->text[0] ) {
			re.SetColor( e->color );
			SCR_DrawStringExt( (int)(e->x), (int)(e->y), e->fontsize, e->text, e->color, qfalse, qfalse );
		}

		re.SetColor( NULL );
	}
}
