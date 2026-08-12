/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "vk_ndgi.h"
#include "vk.h"
#include "tr_sprite_props.h"
#include "tr_decal_props.h"

static int			r_firstSceneDrawSurf;
static int			r_firstSceneLitSurf;

int			r_numdlights;
static int			r_firstSceneDlight;

static int			r_numentities;
static int			r_firstSceneEntity;

static int			r_numpolys;
static int			r_firstScenePoly;

static int			r_numpolyverts;
static int			r_deferredLightDemoLastAdded;

typedef struct {
	const refEntity_t	*ent;
	int					channelCount;
	uint32_t			channelHash[IQM_MORPH_MAX_CHANNELS];
	float				channelWeight[IQM_MORPH_MAX_CHANNELS];
} pendingMorphState_t;

static pendingMorphState_t	r_pendingMorphStates[MAX_REFENTITIES];
static int					r_numPendingMorphStates;

static pendingMorphState_t *R_FindPendingMorphState( const refEntity_t *ent, qboolean create ) {
	int i;

	for ( i = 0; i < r_numPendingMorphStates; i++ ) {
		if ( r_pendingMorphStates[i].ent == ent ) {
			return &r_pendingMorphStates[i];
		}
	}

	if ( !create || r_numPendingMorphStates >= (int)ARRAY_LEN( r_pendingMorphStates ) ) {
		return NULL;
	}

	Com_Memset( &r_pendingMorphStates[r_numPendingMorphStates], 0, sizeof( r_pendingMorphStates[0] ) );
	r_pendingMorphStates[r_numPendingMorphStates].ent = ent;
	r_numPendingMorphStates++;
	return &r_pendingMorphStates[r_numPendingMorphStates - 1];
}

static void R_CopyPendingMorphStateToEntity( const refEntity_t *srcEnt, trRefEntity_t *dstEnt ) {
	const pendingMorphState_t *state = R_FindPendingMorphState( srcEnt, qfalse );
	int i;

	dstEnt->morphChannelCount = 0;
	dstEnt->morphActiveCount = 0;
	dstEnt->morphDebugMaxAbsWeight = 0.0f;
	for ( i = 0; i < IQM_MORPH_MAX_CHANNELS; i++ ) {
		dstEnt->morphChannelWeightPrev[i] = 0.0f;
	}
	for ( i = 0; i < IQM_MORPH_TOP_K; i++ ) {
		dstEnt->morphActiveTargetIndex[i] = -1;
		dstEnt->morphActiveWeight[i] = 0.0f;
		dstEnt->morphGpuWeightPrev[i] = 0.0f;
	}
	dstEnt->morphGpuWeightsPrimedSingleUse = qfalse;

	if ( !state ) {
		return;
	}

	dstEnt->morphChannelCount = state->channelCount;
	for ( i = 0; i < state->channelCount; i++ ) {
		dstEnt->morphChannelHashes[i] = state->channelHash[i];
		dstEnt->morphChannelWeights[i] = state->channelWeight[i];
	}
}

void RE_SetEntityMorphWeight( const refEntity_t *ent, const char *name, float weight ) {
	pendingMorphState_t *state;
	uint32_t hash;
	int i;
	int replaceIndex;
	float replaceAbs;
	float absWeight;

	if ( !ent || !name || !name[0] ) {
		return;
	}

	if ( !tr.registered || !r_morph || !r_morph->integer ) {
		return;
	}

	if ( weight > 2.0f ) {
		weight = 2.0f;
	} else if ( weight < -2.0f ) {
		weight = -2.0f;
	}

	hash = (uint32_t)Com_GenerateHashValue( name, 0x7fffffffU );
	state = R_FindPendingMorphState( ent, qtrue );
	if ( !state ) {
		return;
	}

	for ( i = 0; i < state->channelCount; i++ ) {
		if ( state->channelHash[i] == hash ) {
			state->channelWeight[i] = weight;
			return;
		}
	}

	if ( state->channelCount < IQM_MORPH_MAX_CHANNELS ) {
		state->channelHash[state->channelCount] = hash;
		state->channelWeight[state->channelCount] = weight;
		state->channelCount++;
		return;
	}

	replaceIndex = 0;
	replaceAbs = fabsf( state->channelWeight[0] );
	absWeight = fabsf( weight );
	for ( i = 1; i < state->channelCount; i++ ) {
		float curAbs = fabsf( state->channelWeight[i] );
		if ( curAbs < replaceAbs ) {
			replaceIndex = i;
			replaceAbs = curAbs;
		}
	}

	if ( absWeight > replaceAbs ) {
		state->channelHash[replaceIndex] = hash;
		state->channelWeight[replaceIndex] = weight;
	}
}

/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {

	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;
	r_firstSceneLitSurf = 0;

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
	r_numPendingMorphStates = 0;
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
	r_numPendingMorphStates = 0;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( (surfaceType_t *)poly, sh, poly->fogIndex, 0 );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	const fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}
#if 0
	if ( !hShader ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}
#endif
	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			ri.Printf( PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];
		
		Com_Memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );
#if 0
		if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
			poly->verts->modulate[0] = 255;
			poly->verts->modulate[1] = 255;
			poly->verts->modulate[2] = 255;
			poly->verts->modulate[3] = 255;
		}
#endif
		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex]; 
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}


//=================================================================================

static int isnan_fp( const float *f )
{
	uint32_t u;
	Com_Memcpy( &u, f, sizeof( u ) );
	u = 0x7F800000 - ( u & 0x7FFFFFFF );
	return (int)( u >> 31 );
}


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, qboolean intShaderTime ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= MAX_REFENTITIES ) {
		ri.Printf( PRINT_DEVELOPER, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n" );
		return;
	}
	if ( isnan_fp( &ent->origin[0] ) || isnan_fp( &ent->origin[1] ) || isnan_fp( &ent->origin[2] ) ) {
		static qboolean first_time = qtrue;
		if ( first_time ) {
			first_time = qfalse;
			ri.Printf( PRINT_WARNING, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n" );
		}
		return;
	}
	if ( (unsigned)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].intShaderTime = intShaderTime;
	R_CopyPendingMorphStateToEntity( ent, &backEndData->entities[r_numentities] );

	r_numentities++;
}


/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= (int)ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	if ( r_dlightMode->integer ) {
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
	dl->area = qfalse;
	dl->areaHalfWidth = 0.0f;
	dl->areaHalfHeight = 0.0f;
	VectorSet( dl->areaRight, 1.0f, 0.0f, 0.0f );
	VectorSet( dl->areaUp, 0.0f, 0.0f, 1.0f );
}


/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b  ) {
	dlight_t	*dl;
	if ( VectorCompare( start, end ) ) {
		RE_AddDynamicLightToScene( start, intensity, r, g, b, 0 );
		return;
	}
	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= (int)ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	if ( r_dlightMode->integer ) {
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[ r_numdlights++ ];
	VectorCopy( start, dl->origin );
	VectorCopy( end, dl->origin2 );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
	dl->area = qfalse;
	dl->areaHalfWidth = 0.0f;
	dl->areaHalfHeight = 0.0f;
	VectorSet( dl->areaRight, 1.0f, 0.0f, 0.0f );
	VectorSet( dl->areaUp, 0.0f, 0.0f, 1.0f );
}

/*
=====================
RE_AddAreaLightToScene

Rectangular area light for LTC shading (Forward+ / deferred). Intensity is
legacy-scale luminance; photometric nits convert via vk_photometric when active.
=====================
*/
void RE_AddAreaLightToScene( const vec3_t org, float halfWidth, float halfHeight,
	const vec3_t right, const vec3_t up, float intensity, float r, float g, float b )
{
	dlight_t *dl;
	vec3_t rAxis, uAxis;
	float rLen, uLen;
	float diag;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= (int)ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0.0f || halfWidth <= 0.0f || halfHeight <= 0.0f ) {
		return;
	}

	VectorCopy( right ? right : vec3_origin, rAxis );
	VectorCopy( up ? up : vec3_origin, uAxis );
	rLen = VectorNormalize( rAxis );
	uLen = VectorNormalize( uAxis );
	if ( rLen <= 1e-4f ) {
		VectorSet( rAxis, 1.0f, 0.0f, 0.0f );
	}
	if ( uLen <= 1e-4f ) {
		VectorSet( uAxis, 0.0f, 0.0f, 1.0f );
	}

	if ( r_dlightMode->integer ) {
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	VectorClear( dl->origin2 );
	diag = sqrtf( halfWidth * halfWidth + halfHeight * halfHeight );
	/* radius = cull/influence sphere (same role as point dlight radius). */
	dl->radius = intensity;
	if ( dl->radius < diag * 2.0f ) {
		dl->radius = diag * 2.0f;
	}
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qfalse;
	dl->area = qtrue;
	dl->areaHalfWidth = halfWidth;
	dl->areaHalfHeight = halfHeight;
	VectorCopy( rAxis, dl->areaRight );
	VectorCopy( uAxis, dl->areaUp );
}



/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}


/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}

static void R_AddDeferredLightDemoLights( void )
{
	static const vec3_t colors[8] = {
		{ 1.00f, 0.22f, 0.12f },
		{ 1.00f, 0.70f, 0.18f },
		{ 0.35f, 0.95f, 0.28f },
		{ 0.12f, 0.85f, 1.00f },
		{ 0.32f, 0.42f, 1.00f },
		{ 0.85f, 0.28f, 1.00f },
		{ 1.00f, 0.30f, 0.62f },
		{ 0.90f, 0.95f, 1.00f }
	};
	const int maxAdd = (int)ARRAY_LEN( backEndData->dlights ) - r_numdlights;
	int count, i;
	float radius, phase, distance, energy;

	/* Keep the status counter tied to the main world scene. HUD/weapon/UI
	 * scenes also call this hook and must not erase evidence that the world
	 * received the synthetic lights earlier in the frame. */
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}
	r_deferredLightDemoLastAdded = 0;
	if ( !r_deferredLightDemo || !r_deferredLightDemo->integer ||
		!r_deferredLightDemoCount || r_deferredLightDemoCount->integer <= 0 ) {
		return;
	}

	count = r_deferredLightDemoCount->integer;
	if ( count > 64 ) {
		count = 64;
	}
	if ( count > maxAdd ) {
		count = maxAdd;
	}
	if ( count <= 0 ) {
		return;
	}

	radius = r_deferredLightDemoRadius ? r_deferredLightDemoRadius->value : 360.0f;
	distance = r_deferredLightDemoDistance ?
		Com_Clamp( 160.0f, 1024.0f, r_deferredLightDemoDistance->value ) : 520.0f;
	energy = r_deferredLightDemoEnergy ?
		Com_Clamp( 0.25f, 4.0f, r_deferredLightDemoEnergy->value ) : 1.0f;
	phase = ( r_deferredLightDemoAnimate && r_deferredLightDemoAnimate->integer ) ?
		(float)tr.refdef.floatTime : 0.0f;

	for ( i = 0; i < count; ++i ) {
		vec3_t org;
		const int col = i & 7;
		const int row = i >> 3;
		const float u = (float)col - 3.5f;
		const float v = (float)row - 3.5f;
		const float wave = sinf( phase * 1.7f + (float)i * 0.37f );
		const float swirl = cosf( phase * 1.1f + (float)i * 0.23f );
		const float colorPulse = 0.75f + 0.25f * sinf( phase * 2.3f + (float)i );

		VectorMA( tr.refdef.vieworg, distance + 38.0f * v, tr.refdef.viewaxis[0], org );
		VectorMA( org, u * 145.0f + swirl * 34.0f, tr.refdef.viewaxis[1], org );
		VectorMA( org, 48.0f + v * 32.0f + wave * 70.0f, tr.refdef.viewaxis[2], org );

		RE_AddDynamicLightToScene( org, radius, colors[i & 7][0] * colorPulse * energy,
			colors[i & 7][1] * colorPulse * energy, colors[i & 7][2] * colorPulse * energy, qfalse );
		r_deferredLightDemoLastAdded++;
	}
}

void R_DeferredLightDemoStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"deferred many lights: enabled=%d requested=%d lastAdded=%d radius=%.1f distance=%.1f energy=%.2f animate=%d "
		"renderMode=%d deferredLighting=%d forwardPlus=%d\n",
		r_deferredLightDemo ? r_deferredLightDemo->integer : 0,
		r_deferredLightDemoCount ? r_deferredLightDemoCount->integer : 0,
		r_deferredLightDemoLastAdded,
		r_deferredLightDemoRadius ? r_deferredLightDemoRadius->value : 0.0f,
		r_deferredLightDemoDistance ? r_deferredLightDemoDistance->value : 520.0f,
		r_deferredLightDemoEnergy ? r_deferredLightDemoEnergy->value : 1.0f,
		r_deferredLightDemoAnimate ? r_deferredLightDemoAnimate->integer : 0,
		r_renderMode ? r_renderMode->integer : -1,
		r_deferredLighting ? r_deferredLighting->integer : 0,
		r_forwardPlus ? r_forwardPlus->integer : 0 );
}


void *R_GetCommandBuffer( int bytes );

void RE_BeginScene( const refdef_t *fd ) {
	Com_Memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		const int *srcMask = (const int *)fd->areamask;
		int *dstMask = (int *)tr.refdef.areamask;
		for ( i = 0; i < (int)( MAX_MAP_AREA_BYTES / sizeof( int ) ); i++ ) {
			areaDiff |= dstMask[i] ^ srcMask[i];
			dstMask[i] = srcMask[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = (double)tr.refdef.time * 0.001; // -EC-: cast to double

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	R_AddDeferredLightDemoLights();
	R_SourceEntities_AddLights();

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled
	if ( r_dynamiclight->integer == 0 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;
}

void RE_EndScene( void ) {
	/* After this view's entities are finalized, stash morph weights for next frame's GPU motion vectors. */
	vk_snap_gpu_morph_weights_for_motion();
	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;

	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;
}

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd ) {
	renderCommand_t	lastRenderCommand;
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}

	if ( r_norefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		R_SpriteProps_AddRefEntitiesToScene( fd->time );
		R_DecalProps_AddRefEntitiesToScene( fd->time );
		R_NDGI_FrameUpdate();
	}

	RE_BeginScene( fd );

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;

	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;
	
	parms.stereoFrame = tr.refdef.stereoFrame;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

	lastRenderCommand = tr.lastRenderCommand;
	tr.drawSurfCmd = NULL;
	tr.numDrawSurfCmds = 0;

	R_RenderView( &parms );

	if ( tr.needScreenMap )
	{
		if ( lastRenderCommand == RC_DRAW_BUFFER )
		{
			// duplicate all views, including portals
			drawSurfsCommand_t *cmd, *src = NULL;
			int i;

			for ( i = 0; i < tr.numDrawSurfCmds; i++ )
			{
				cmd = R_GetCommandBuffer( sizeof( *cmd ) );
				if ( cmd )
				{
					src = tr.drawSurfCmd + i;
					*cmd = *src;
				}
				else
				{
					break;
				}
			}

			if ( src )
			{
				// first drawsurface
				tr.drawSurfCmd[0].refdef.needScreenMap = qtrue;
				// last drawsurface
				src->refdef.switchRenderPass = qtrue;
			}
		}

		tr.needScreenMap = 0;
	}

	RE_EndScene();

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
