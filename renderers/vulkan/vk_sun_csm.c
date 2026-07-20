/*
===========================================================================
Cascaded sun shadows — CVars and split helpers (Raster Ultra 1.1).
===========================================================================
*/

#include "tr_local.h"
#include "vk_sun_csm.h"

static cvar_t *r_sunShadowCascades;
static cvar_t *r_sunShadowDistance;
static cvar_t *r_sunShadowSplitLambda;
static cvar_t *r_sunShadowCascadeBlend;
static cvar_t *r_sunShadowStable;
static cvar_t *r_sunShadowDebug;
static qboolean s_inited;

void VK_SunCSM_Init( void )
{
	if ( s_inited ) {
		return;
	}

	r_sunShadowCascades = ri.Cvar_Get( "r_sunShadowCascades", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_sunShadowCascades, "1", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_sunShadowCascades,
		"Directional sun shadow cascades (latched; recreate shadow map):\n"
		" 1 - legacy single cascade (default, certified)\n"
		" 2–4 - atlas CSM (Raster Ultra). Requires r_pbrSunShadow 1." );
	ri.Cvar_SetGroup( r_sunShadowCascades, CVG_RENDERER );

	r_sunShadowDistance = ri.Cvar_Get( "r_sunShadowDistance", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunShadowDistance, "0", "100000", CV_FLOAT );
	ri.Cvar_SetDescription( r_sunShadowDistance,
		"Max sun-shadow distance (0 = use r_fogShadowMaxDistance / view zFar)." );
	ri.Cvar_SetGroup( r_sunShadowDistance, CVG_RENDERER );

	r_sunShadowSplitLambda = ri.Cvar_Get( "r_sunShadowSplitLambda", "0.75", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunShadowSplitLambda, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_sunShadowSplitLambda,
		"PSSM mix: 0=linear splits, 1=logarithmic (practical cascade distribution)." );
	ri.Cvar_SetGroup( r_sunShadowSplitLambda, CVG_RENDERER );

	r_sunShadowCascadeBlend = ri.Cvar_Get( "r_sunShadowCascadeBlend", "0.08", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunShadowCascadeBlend, "0", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_sunShadowCascadeBlend,
		"Cascade overlap blend fraction of each split interval (0=hard seams)." );
	ri.Cvar_SetGroup( r_sunShadowCascadeBlend, CVG_RENDERER );

	r_sunShadowStable = ri.Cvar_Get( "r_sunShadowStable", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunShadowStable, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_sunShadowStable,
		"Texel-snap cascade projections (uses r_fogShadowSnap when set)." );
	ri.Cvar_SetGroup( r_sunShadowStable, CVG_RENDERER );

	r_sunShadowDebug = ri.Cvar_Get( "r_sunShadowDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunShadowDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_sunShadowDebug,
		"Sun CSM debug: 0=off, 1=cascade index tint, 2=log cascade stats." );
	ri.Cvar_SetGroup( r_sunShadowDebug, CVG_RENDERER );

	s_inited = qtrue;
	ri.Printf( PRINT_ALL, "[VK][CSM] cascades=%d distance=%.0f lambda=%.2f blend=%.2f\n",
		r_sunShadowCascades->integer,
		r_sunShadowDistance->value,
		r_sunShadowSplitLambda->value,
		r_sunShadowCascadeBlend->value );
}

void VK_SunCSM_Shutdown( void )
{
	s_inited = qfalse;
}

int VK_SunCSM_CascadeCount( void )
{
	int n = r_sunShadowCascades ? r_sunShadowCascades->integer : 1;
	if ( n < 1 ) {
		n = 1;
	}
	if ( n > VK_SUN_CSM_MAX ) {
		n = VK_SUN_CSM_MAX;
	}
	return n;
}

float VK_SunCSM_SplitLambda( void )
{
	return r_sunShadowSplitLambda ? Com_Clamp( 0.0f, 1.0f, r_sunShadowSplitLambda->value ) : 0.75f;
}

float VK_SunCSM_MaxDistance( void )
{
	if ( r_sunShadowDistance && r_sunShadowDistance->value > 1.0f ) {
		return r_sunShadowDistance->value;
	}
	if ( r_fogShadowMaxDistance && r_fogShadowMaxDistance->value > 1.0f ) {
		return r_fogShadowMaxDistance->value;
	}
	return 0.0f; /* caller uses view zFar */
}

float VK_SunCSM_CascadeBlend( void )
{
	return r_sunShadowCascadeBlend ? Com_Clamp( 0.0f, 0.5f, r_sunShadowCascadeBlend->value ) : 0.08f;
}

qboolean VK_SunCSM_Stable( void )
{
	if ( r_sunShadowStable && !r_sunShadowStable->integer ) {
		return qfalse;
	}
	return ( !r_fogShadowSnap || r_fogShadowSnap->integer ) ? qtrue : qfalse;
}

int VK_SunCSM_Debug( void )
{
	return r_sunShadowDebug ? r_sunShadowDebug->integer : 0;
}

void VK_SunCSM_ComputeSplits( float nearPlane, float farPlane, int cascadeCount,
	float lambda, float *splitsOut )
{
	int i;
	float ratio;
	float inv;

	if ( !splitsOut || cascadeCount < 1 ) {
		return;
	}
	if ( nearPlane < 0.1f ) {
		nearPlane = 0.1f;
	}
	if ( farPlane <= nearPlane + 1.0f ) {
		farPlane = nearPlane + 1.0f;
	}
	lambda = Com_Clamp( 0.0f, 1.0f, lambda );
	ratio = farPlane / nearPlane;
	inv = 1.0f / (float)cascadeCount;

	for ( i = 0; i < cascadeCount; i++ ) {
		float p = (float)( i + 1 ) * inv;
		float logSplit = nearPlane * powf( ratio, p );
		float linSplit = nearPlane + ( farPlane - nearPlane ) * p;
		splitsOut[i] = linSplit * ( 1.0f - lambda ) + logSplit * lambda;
	}
	splitsOut[cascadeCount - 1] = farPlane;
}

void VK_SunCSM_AtlasTile( int cascade, int cascadeCount, int mapSize,
	int *outX, int *outY, int *outTileSize, int *outAtlasSize )
{
	int grid = 1;
	int tile;
	int atlas;

	if ( cascadeCount <= 1 ) {
		grid = 1;
	} else {
		grid = 2; /* 2–4 cascades share a 2×2 atlas */
	}
	tile = mapSize;
	atlas = mapSize * grid;
	if ( outTileSize ) {
		*outTileSize = tile;
	}
	if ( outAtlasSize ) {
		*outAtlasSize = atlas;
	}
	if ( outX ) {
		*outX = ( cascade % grid ) * tile;
	}
	if ( outY ) {
		*outY = ( cascade / grid ) * tile;
	}
}
