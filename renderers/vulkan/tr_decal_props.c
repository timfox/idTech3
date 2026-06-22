/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Parses misc_decal map entities and submits polygon-offset RT_SPRITE decals.
===========================================================================
*/

#include "tr_local.h"
#include "tr_decal_props.h"
#include "../../qcommon/engine_decal_map.h"

#define MAX_DECAL_PROPS 1024

typedef struct decalProp_s {
	vec3_t		origin;
	float		radius;
	float		pitch;
	float		yaw;
	qhandle_t	shader;
} decalProp_t;

static decalProp_t decalProps[MAX_DECAL_PROPS];
static int decalPropCount;

static cvar_t *r_decalProps;
static cvar_t *r_decalPropsMapParse;

static qhandle_t DP_RegisterShader( const char *name ) {
	if ( !name || !name[0] || !tr.registered ) {
		return 0;
	}
	return RE_RegisterShader( name );
}

static void DP_FillRefEntity( const decalProp_t *prop, refEntity_t *ent ) {
	vec3_t angles;

	Com_Memset( ent, 0, sizeof( *ent ) );
	ent->reType = RT_SPRITE;
	ent->renderfx |= RF_DEPTHHACK;
	ent->customShader = prop->shader;
	VectorCopy( prop->origin, ent->origin );
	ent->radius = prop->radius;
	ent->rotation = prop->yaw;
	/* Static decals: stable temporal history (TAA / motion vectors) */
	ent->frame = 0;
	ent->oldframe = 0;
	ent->backlerp = 0.0f;
	ent->shader.rgba[0] = 255;
	ent->shader.rgba[1] = 255;
	ent->shader.rgba[2] = 255;
	ent->shader.rgba[3] = 255;

	angles[0] = prop->pitch;
	angles[1] = prop->yaw;
	angles[2] = 0.0f;
	AnglesToAxis( angles, ent->axis );
}

void R_DecalProps_Clear( void ) {
	decalPropCount = 0;
	Com_Memset( decalProps, 0, sizeof( decalProps ) );
}

void R_DecalProps_ParseFromEntityString( const char *entityString ) {
	engineDecalMapList_t list;
	int i;

	if ( !entityString || !entityString[0] ) {
		return;
	}
	if ( r_decalProps && !r_decalProps->integer ) {
		return;
	}
	if ( r_decalPropsMapParse && !r_decalPropsMapParse->integer ) {
		return;
	}

	R_DecalProps_Clear();
	EngineDecalMap_Parse( entityString, &list );

	for ( i = 0; i < list.count && decalPropCount < MAX_DECAL_PROPS; i++ ) {
		const engineDecalMapDef_t *def = &list.defs[i];
		decalProp_t *prop = &decalProps[decalPropCount];

		prop->shader = DP_RegisterShader( def->shader );
		if ( !prop->shader ) {
			continue;
		}
		VectorCopy( def->origin, prop->origin );
		prop->radius = def->radius;
		prop->pitch = def->pitch;
		prop->yaw = def->yaw;
		decalPropCount++;
	}

	if ( decalPropCount > 0 ) {
		ri.Printf( PRINT_ALL, "[engine][decals] %d map decal props parsed\n", decalPropCount );
	}
}

void RE_AddEngineDecalToScene( const engineDecalDesc_t *desc ) {
	refEntity_t ent;
	decalProp_t prop;

	if ( !desc || !desc->shader ) {
		return;
	}

	Com_Memset( &prop, 0, sizeof( prop ) );
	VectorCopy( desc->origin, prop.origin );
	prop.radius = desc->radius > 0.0f ? desc->radius : 32.0f;
	prop.pitch = desc->pitch;
	prop.yaw = desc->yaw;
	prop.shader = desc->shader;

	DP_FillRefEntity( &prop, &ent );
	RE_AddRefEntityToScene( &ent, qfalse );
}

void R_DecalProps_AddRefEntitiesToScene( int refdefTimeMs ) {
	int i;

	(void)refdefTimeMs;

	if ( !r_decalProps || !r_decalProps->integer || decalPropCount <= 0 ) {
		return;
	}
	if ( r_decalPropsMapParse && !r_decalPropsMapParse->integer ) {
		return;
	}
	if ( !tr.registered ) {
		return;
	}

	for ( i = 0; i < decalPropCount; i++ ) {
		refEntity_t ent;

		DP_FillRefEntity( &decalProps[i], &ent );
		RE_AddRefEntityToScene( &ent, qfalse );
	}
}

void R_DecalProps_Init( void ) {
	r_decalProps = ri.Cvar_Get( "r_decalProps", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_decalProps,
		"Engine-native map decal props (misc_decal)." );
	ri.Cvar_SetGroup( r_decalProps, CVG_RENDERER );
	r_decalPropsMapParse = ri.Cvar_Get( "r_decalPropsMapParse", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_decalPropsMapParse,
		"Parse misc_decal from BSP entity lump (auto 0 when CS_ENGINE_DECAL_META set)." );
	ri.Cvar_SetGroup( r_decalPropsMapParse, CVG_RENDERER );
	if ( r_decalProps->integer ) {
		ri.Printf( PRINT_ALL, "[engine][decals] r_decalProps=1\n" );
	}
}
