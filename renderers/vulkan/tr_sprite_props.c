/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Parses misc_billboard / misc_flipbook / misc_imposter / misc_voxel map entities
and submits RT_SPRITE (billboards) or RT_MODEL (voxels) each frame.
Games may also call RE_AddEngineSpriteToScene.
When the server spawns map sprites into snapshots, set r_spritePropsMapParse 0
(or let cl_engineSprites sync via CS_ENGINE_SPRITE_META).
===========================================================================
*/

#include "tr_local.h"
#include "tr_sprite_props.h"
#include "engine_sprite_map.h"

#define MAX_SPRITE_PROPS 2048

typedef struct spriteProp_s {
	engineSpriteType_t	type;
	vec3_t				origin;
	float				radius;
	float				rotation;
	qhandle_t			shader;
	qhandle_t			hModel;
	int					cols;
	int					rows;
	float				fps;
	float				swayAmount;
	float				swaySpeed;
} spriteProp_t;

static spriteProp_t spriteProps[MAX_SPRITE_PROPS];
static int spritePropCount;

static cvar_t *r_spriteProps;
static cvar_t *r_spritePropsMapParse;

static qhandle_t SP_RegisterShader( const char *name ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	if ( !tr.registered ) {
		return 0;
	}
	return RE_RegisterShader( name );
}

static void SP_AddPropFromDef( const engineSpriteMapDef_t *def ) {
	spriteProp_t prop;

	if ( !def || !def->shader[0] || spritePropCount >= MAX_SPRITE_PROPS ) {
		return;
	}

	Com_Memset( &prop, 0, sizeof( prop ) );
	prop.type = def->type;
	VectorCopy( def->origin, prop.origin );
	prop.radius = def->radius;
	prop.rotation = def->rotation;
	prop.cols = def->cols;
	prop.rows = def->rows;
	prop.fps = def->fps;
	prop.swayAmount = def->swayAmount;
	prop.swaySpeed = def->swaySpeed;

	if ( def->type == ENGINE_SPRITE_VOXEL ) {
		prop.hModel = RE_RegisterModel( def->shader );
		if ( !prop.hModel ) {
			ri.Printf( PRINT_WARNING, "[engine][sprites] misc_voxel: failed to load '%s'\n", def->shader );
			return;
		}
		if ( prop.radius <= 0.0f ) {
			prop.radius = 1.0f;
		}
	} else {
		prop.shader = SP_RegisterShader( def->shader );
		if ( !prop.shader ) {
			return;
		}
	}

	spriteProps[spritePropCount++] = prop;
}

void R_SpriteProps_Clear( void ) {
	spritePropCount = 0;
	Com_Memset( spriteProps, 0, sizeof( spriteProps ) );
}

void R_SpriteProps_ParseFromEntityString( const char *entityString ) {
	engineSpriteMapList_t list;
	int i;
	int parsedBillboards = 0;
	int parsedFlipbooks = 0;
	int parsedImposters = 0;
	int parsedVoxels = 0;

	if ( !entityString || !entityString[0] ) {
		return;
	}
	if ( r_spriteProps && !r_spriteProps->integer ) {
		return;
	}
	if ( r_spritePropsMapParse && !r_spritePropsMapParse->integer ) {
		return;
	}

	R_SpriteProps_Clear();
	EngineSpriteMap_Parse( entityString, &list );

	for ( i = 0; i < list.count; i++ ) {
		const engineSpriteMapDef_t *def = &list.defs[i];

		SP_AddPropFromDef( def );
		switch ( def->type ) {
		case ENGINE_SPRITE_BILLBOARD:
			parsedBillboards++;
			break;
		case ENGINE_SPRITE_FLIPBOOK:
			parsedFlipbooks++;
			break;
		case ENGINE_SPRITE_IMPOSTER:
			parsedImposters++;
			break;
		case ENGINE_SPRITE_VOXEL:
			parsedVoxels++;
			break;
		}
	}

	if ( spritePropCount > 0 ) {
		ri.Printf( PRINT_ALL,
			"[engine][sprites] %d map props (billboard=%d flipbook=%d imposter=%d voxel=%d)\n",
			spritePropCount, parsedBillboards, parsedFlipbooks, parsedImposters, parsedVoxels );
	}
}

static void SP_FillVoxelRefEntity( const vec3_t origin, float scale, float yawDeg,
	qhandle_t hModel, refEntity_t *ent )
{
	vec3_t angles;

	Com_Memset( ent, 0, sizeof( *ent ) );
	ent->reType = RT_MODEL;
	ent->hModel = hModel;
	VectorCopy( origin, ent->origin );
	VectorCopy( origin, ent->lightingOrigin );
	ent->shader.rgba[0] = 255;
	ent->shader.rgba[1] = 255;
	ent->shader.rgba[2] = 255;
	ent->shader.rgba[3] = 255;

	angles[0] = 0.0f;
	angles[1] = yawDeg;
	angles[2] = 0.0f;
	AnglesToAxis( angles, ent->axis );
	if ( scale != 1.0f && scale > 0.0f ) {
		VectorScale( ent->axis[0], scale, ent->axis[0] );
		VectorScale( ent->axis[1], scale, ent->axis[1] );
		VectorScale( ent->axis[2], scale, ent->axis[2] );
		ent->nonNormalizedAxes = qtrue;
	}
}

static void SP_FillRefEntityFromDesc( engineSpriteType_t type, const vec3_t origin,
	float radius, float rotation, qhandle_t shader, qhandle_t hModel,
	int cols, int rows, float fps,
	float swayAmount, float swaySpeed, refEntity_t *ent, int refdefTimeMs )
{
	int totalFrames;
	int frameIndex;
	float rot;

	if ( type == ENGINE_SPRITE_VOXEL ) {
		SP_FillVoxelRefEntity( origin, radius, rotation, hModel, ent );
		return;
	}

	Com_Memset( ent, 0, sizeof( *ent ) );
	ent->reType = RT_SPRITE;
	ent->customShader = shader;
	VectorCopy( origin, ent->origin );
	ent->radius = radius;
	ent->shader.rgba[0] = 255;
	ent->shader.rgba[1] = 255;
	ent->shader.rgba[2] = 255;
	ent->shader.rgba[3] = 255;

	rot = rotation;
	if ( swayAmount > 0.0f ) {
		float t = (float)refdefTimeMs * 0.001f * swaySpeed;
		rot += sin( t ) * swayAmount;
	}
	ent->rotation = rot;

	switch ( type ) {
	case ENGINE_SPRITE_IMPOSTER:
		ent->renderfx |= RF_SPRITE_YAWLOCK;
		break;
	case ENGINE_SPRITE_FLIPBOOK:
		ent->renderfx |= RF_SPRITE_FLIPBOOK;
		totalFrames = cols * rows;
		if ( totalFrames < 1 ) {
			totalFrames = 1;
		}
		frameIndex = (int)( (float)refdefTimeMs * fps * 0.001f );
		if ( frameIndex < 0 ) {
			frameIndex = 0;
		}
		ent->frame = frameIndex % totalFrames;
		ent->oldframe = cols;
		ent->skinNum = rows;
		break;
	default:
		break;
	}
}

static void SP_FillRefEntity( const spriteProp_t *prop, refEntity_t *ent, int refdefTimeMs ) {
	SP_FillRefEntityFromDesc( prop->type, prop->origin, prop->radius, prop->rotation,
		prop->shader, prop->hModel, prop->cols, prop->rows, prop->fps,
		prop->swayAmount, prop->swaySpeed, ent, refdefTimeMs );
}

void RE_AddEngineSpriteToScene( const engineSpriteDesc_t *desc ) {
	int refdefTimeMs = tr.refdef.time;
	RE_AddEngineSpriteToSceneAtTime( desc, refdefTimeMs );
}

void RE_AddEngineSpriteToSceneAtTime( const engineSpriteDesc_t *desc, int refdefTimeMs ) {
	refEntity_t ent;

	if ( !desc ) {
		return;
	}
	if ( desc->type == ENGINE_SPRITE_VOXEL ) {
		if ( !desc->hModel ) {
			return;
		}
	} else if ( !desc->shader ) {
		return;
	}

	SP_FillRefEntityFromDesc( desc->type, desc->origin, desc->radius, desc->rotation,
		desc->shader, desc->hModel, desc->cols, desc->rows, desc->fps,
		desc->swayAmount, desc->swaySpeed, &ent, refdefTimeMs );
	RE_AddRefEntityToScene( &ent, qfalse );
}

void R_SpriteProps_AddRefEntitiesToScene( int refdefTimeMs ) {
	int i;

	if ( !r_spriteProps || !r_spriteProps->integer || spritePropCount <= 0 ) {
		return;
	}
	if ( r_spritePropsMapParse && !r_spritePropsMapParse->integer ) {
		return;
	}
	if ( !tr.registered ) {
		return;
	}

	for ( i = 0; i < spritePropCount; i++ ) {
		refEntity_t ent;

		SP_FillRefEntity( &spriteProps[i], &ent, refdefTimeMs );
		RE_AddRefEntityToScene( &ent, qfalse );
	}
}

const char *R_MapProps_EntityString( void ) {
	if ( tr.world && tr.world->entityString ) {
		return tr.world->entityString;
	}
	return NULL;
}

void R_SpriteProps_Init( void ) {
	r_spriteProps = ri.Cvar_Get( "r_spriteProps", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_spriteProps,
		"Engine-native map sprite props (misc_billboard, misc_flipbook, misc_imposter, misc_voxel)." );
	ri.Cvar_SetGroup( r_spriteProps, CVG_RENDERER );

	r_spritePropsMapParse = ri.Cvar_Get( "r_spritePropsMapParse", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_spritePropsMapParse,
		"Parse misc_* sprite entities from the client BSP lump. Set 0 when the server "
		"sends map sprites via snapshots (CS_ENGINE_SPRITE_META) to avoid duplicate draws." );
	ri.Cvar_SetGroup( r_spritePropsMapParse, CVG_RENDERER );

	if ( r_spriteProps && r_spriteProps->integer ) {
		ri.Printf( PRINT_ALL, "[engine][sprites] r_spriteProps=1 (map billboards/flipbooks/imposters/voxels)\n" );
	}
}
