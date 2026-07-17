/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "engine_sprite_map.h"
#include "qcommon.h"

static float EngineSpriteMap_Float( const char *value, float def ) {
	if ( !value || !value[0] ) {
		return def;
	}
	return (float)atof( value );
}

static int EngineSpriteMap_Int( const char *value, int def ) {
	if ( !value || !value[0] ) {
		return def;
	}
	return atoi( value );
}

void EngineSpriteMap_Clear( engineSpriteMapList_t *list ) {
	if ( !list ) {
		return;
	}
	list->count = 0;
	Com_Memset( list->defs, 0, sizeof( list->defs ) );
}

void EngineSpriteMap_Parse( const char *entityString, engineSpriteMapList_t *list ) {
	const char *p;
	char key[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];
	char classname[MAX_TOKEN_CHARS];

	if ( !list ) {
		return;
	}

	EngineSpriteMap_Clear( list );

	if ( !entityString || !entityString[0] ) {
		return;
	}

	p = entityString;

	while ( 1 ) {
		const char *token = COM_Parse( &p );
		engineSpriteMapDef_t def;

		if ( !token[0] ) {
			break;
		}
		if ( token[0] != '{' ) {
			continue;
		}

		Com_Memset( &def, 0, sizeof( def ) );
		def.type = ENGINE_SPRITE_BILLBOARD;
		def.radius = 32.0f;
		def.cols = 1;
		def.rows = 1;
		def.fps = 8.0f;
		def.swaySpeed = 1.0f;
		classname[0] = '\0';
		def.shader[0] = '\0';
		{
			qboolean scaleSet = qfalse;

		while ( 1 ) {
			token = COM_Parse( &p );
			if ( !token[0] || token[0] == '}' ) {
				break;
			}

			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_Parse( &p );
			Q_strncpyz( value, token, sizeof( value ) );

			if ( !Q_stricmp( key, "classname" ) ) {
				Q_strncpyz( classname, value, sizeof( classname ) );
			} else if ( !Q_stricmp( key, "origin" ) ) {
				sscanf( value, "%f %f %f", &def.origin[0], &def.origin[1], &def.origin[2] );
			} else if ( !Q_stricmp( key, "shader" ) || !Q_stricmp( key, "model" ) ) {
				Q_strncpyz( def.shader, value, sizeof( def.shader ) );
			} else if ( !Q_stricmp( key, "scale" ) || !Q_stricmp( key, "radius" ) ) {
				def.radius = EngineSpriteMap_Float( value, def.radius );
				scaleSet = qtrue;
			} else if ( !Q_stricmp( key, "rotation" ) || !Q_stricmp( key, "angle" ) ) {
				def.rotation = EngineSpriteMap_Float( value, 0.0f );
			} else if ( !Q_stricmp( key, "cols" ) || !Q_stricmp( key, "columns" ) ) {
				def.cols = EngineSpriteMap_Int( value, 1 );
			} else if ( !Q_stricmp( key, "rows" ) ) {
				def.rows = EngineSpriteMap_Int( value, 1 );
			} else if ( !Q_stricmp( key, "fps" ) || !Q_stricmp( key, "speed" ) ) {
				def.fps = EngineSpriteMap_Float( value, def.fps );
			} else if ( !Q_stricmp( key, "sway" ) ) {
				def.swayAmount = EngineSpriteMap_Float( value, 0.0f );
			} else if ( !Q_stricmp( key, "sway_speed" ) ) {
				def.swaySpeed = EngineSpriteMap_Float( value, def.swaySpeed );
			}
		}

		if ( !classname[0] || !def.shader[0] ) {
			continue;
		}
		if ( list->count >= MAX_ENGINE_MAP_SPRITES ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: MAX_ENGINE_MAP_SPRITES hit; skipping '%s'\n", classname );
			continue;
		}

		if ( !Q_stricmp( classname, "misc_billboard" ) ) {
			def.type = ENGINE_SPRITE_BILLBOARD;
			list->defs[list->count++] = def;
		} else if ( !Q_stricmp( classname, "misc_flipbook" ) ) {
			def.type = ENGINE_SPRITE_FLIPBOOK;
			if ( def.cols < 1 ) {
				def.cols = 1;
			}
			if ( def.rows < 1 ) {
				def.rows = 1;
			}
			if ( def.fps <= 0.0f ) {
				def.fps = 8.0f;
			}
			list->defs[list->count++] = def;
		} else if ( !Q_stricmp( classname, "misc_imposter" ) ) {
			def.type = ENGINE_SPRITE_IMPOSTER;
			list->defs[list->count++] = def;
		} else if ( !Q_stricmp( classname, "misc_voxel" ) ) {
			def.type = ENGINE_SPRITE_VOXEL;
			if ( !scaleSet ) {
				def.radius = 1.0f;
			}
			if ( def.radius <= 0.0f ) {
				def.radius = 1.0f;
			}
			list->defs[list->count++] = def;
		}
		} /* scaleSet scope */
	}
}
