/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "engine_decal_map.h"
#include "qcommon.h"

static float EngineDecalMap_Float( const char *value, float def ) {
	if ( !value || !value[0] ) {
		return def;
	}
	return (float)atof( value );
}

void EngineDecalMap_Clear( engineDecalMapList_t *list ) {
	if ( !list ) {
		return;
	}
	list->count = 0;
	Com_Memset( list->defs, 0, sizeof( list->defs ) );
}

void EngineDecalMap_Parse( const char *entityString, engineDecalMapList_t *list ) {
	const char *p;
	char key[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];
	char classname[MAX_TOKEN_CHARS];

	if ( !list ) {
		return;
	}

	EngineDecalMap_Clear( list );

	if ( !entityString || !entityString[0] ) {
		return;
	}

	p = entityString;

	while ( 1 ) {
		const char *token = COM_Parse( &p );
		engineDecalMapDef_t def;

		if ( !token[0] ) {
			break;
		}
		if ( token[0] != '{' ) {
			continue;
		}

		Com_Memset( &def, 0, sizeof( def ) );
		def.radius = 32.0f;
		def.fadeSec = 0.0f;
		classname[0] = '\0';
		def.shader[0] = '\0';

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
			} else if ( !Q_stricmp( key, "shader" ) || !Q_stricmp( key, "texture" ) ) {
				Q_strncpyz( def.shader, value, sizeof( def.shader ) );
			} else if ( !Q_stricmp( key, "scale" ) || !Q_stricmp( key, "radius" ) || !Q_stricmp( key, "size" ) ) {
				def.radius = EngineDecalMap_Float( value, def.radius );
			} else if ( !Q_stricmp( key, "pitch" ) || !Q_stricmp( key, "angle_pitch" ) ) {
				def.pitch = EngineDecalMap_Float( value, 0.0f );
			} else if ( !Q_stricmp( key, "yaw" ) || !Q_stricmp( key, "angle" ) || !Q_stricmp( key, "angles" ) ) {
				def.yaw = EngineDecalMap_Float( value, 0.0f );
			} else if ( !Q_stricmp( key, "fade" ) || !Q_stricmp( key, "fadetime" ) ) {
				def.fadeSec = EngineDecalMap_Float( value, 0.0f );
			}
		}

		if ( Q_stricmp( classname, "misc_decal" ) ) {
			continue;
		}
		if ( !def.shader[0] ) {
			continue;
		}
		if ( list->count >= MAX_ENGINE_MAP_DECALS ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: MAX_ENGINE_MAP_DECALS hit\n" );
			break;
		}
		list->defs[list->count++] = def;
	}
}
