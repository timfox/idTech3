/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared parser for misc_phys_* map entities (Soft Step / Box3D props).
===========================================================================
*/

#include "engine_phys_map.h"
#include "qcommon.h"

static float EnginePhysMap_Float( const char *value, float def ) {
	if ( !value || !value[0] ) {
		return def;
	}
	return (float)atof( value );
}

void EnginePhysMap_Clear( enginePhysMapList_t *list ) {
	if ( !list ) {
		return;
	}
	list->count = 0;
	Com_Memset( list->defs, 0, sizeof( list->defs ) );
}

void EnginePhysMap_Parse( const char *entityString, enginePhysMapList_t *list ) {
	const char *p;
	char key[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];
	char classname[MAX_TOKEN_CHARS];

	if ( !list ) {
		return;
	}
	EnginePhysMap_Clear( list );
	if ( !entityString || !entityString[0] ) {
		return;
	}

	p = entityString;
	while ( 1 ) {
		const char *token = COM_Parse( &p );
		enginePhysMapDef_t def;

		if ( !token[0] ) {
			break;
		}
		if ( token[0] != '{' ) {
			continue;
		}

		Com_Memset( &def, 0, sizeof( def ) );
		def.type = ENGINE_PHYS_BOX;
		def.mass = 20.0f;
		def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = 16.0f;
		def.radius = 16.0f;
		def.sliderUpper = 96.0f;
		def.axis[2] = 1.0f;
		classname[0] = '\0';

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
			} else if ( !Q_stricmp( key, "angles" ) ) {
				sscanf( value, "%f %f %f", &def.angles[0], &def.angles[1], &def.angles[2] );
			} else if ( !Q_stricmp( key, "_size" ) || !Q_stricmp( key, "size" ) ) {
				float s = EnginePhysMap_Float( value, 16.0f );
				def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = s * 0.5f;
				def.radius = s * 0.5f;
			} else if ( !Q_stricmp( key, "mass" ) ) {
				def.mass = EnginePhysMap_Float( value, 20.0f );
			} else if ( !Q_stricmp( key, "material" ) ) {
				def.materialId = atoi( value );
			} else if ( !Q_stricmp( key, "lip" ) || !Q_stricmp( key, "height" ) ) {
				def.sliderUpper = EnginePhysMap_Float( value, 96.0f );
			} else if ( !Q_stricmp( key, "angle" ) ) {
				/* Quake angle → yaw only; elevators use axis */
				def.angles[1] = EnginePhysMap_Float( value, 0.0f );
			} else if ( !Q_stricmp( key, "targetname" ) ) {
				Q_strncpyz( def.targetname, value, sizeof( def.targetname ) );
			} else if ( !Q_stricmp( key, "model" ) || !Q_stricmp( key, "rag" ) ) {
				Q_strncpyz( def.model, value, sizeof( def.model ) );
			} else if ( !Q_stricmp( key, "dead" ) || !Q_stricmp( key, "startDead" ) ) {
				def.ragdollDead = atoi( value ) ? qtrue : qfalse;
			}
		}

		if ( !Q_stricmp( classname, "misc_phys_box" ) ) {
			def.type = ENGINE_PHYS_BOX;
		} else if ( !Q_stricmp( classname, "misc_phys_sphere" ) ) {
			def.type = ENGINE_PHYS_SPHERE;
		} else if ( !Q_stricmp( classname, "misc_phys_static" ) ) {
			def.type = ENGINE_PHYS_STATIC;
		} else if ( !Q_stricmp( classname, "misc_phys_sensor" ) ) {
			def.type = ENGINE_PHYS_SENSOR;
		} else if ( !Q_stricmp( classname, "misc_phys_slider" ) ) {
			def.type = ENGINE_PHYS_SLIDER;
		} else if ( !Q_stricmp( classname, "misc_phys_ragdoll" ) ) {
			def.type = ENGINE_PHYS_RAGDOLL;
		} else if ( !Q_stricmp( classname, "misc_phys_dmm" )
			|| !Q_stricmp( classname, "func_destructible" ) ) {
			def.type = ENGINE_PHYS_DMM;
		} else {
			continue;
		}

		if ( list->count >= ENGINE_PHYS_MAP_MAX ) {
			Com_Printf( S_COLOR_YELLOW "EnginePhysMap: max %d props, truncating\n", ENGINE_PHYS_MAP_MAX );
			break;
		}
		list->defs[list->count++] = def;
	}
}
