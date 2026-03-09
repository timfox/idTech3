#ifndef VK_VOLUMETRIC_PARAMS_H
#define VK_VOLUMETRIC_PARAMS_H

#include <stddef.h>

#define VK_FROXEL_DEFAULT_WIDTH 160
#define VK_FROXEL_DEFAULT_HEIGHT 90
#define VK_FROXEL_DEFAULT_SLICES 96
#define VK_FLUID_DEFAULT_RESOLUTION_SCALE 0.5f
#define VK_FLUID_MAX_PRESSURE_ITERATIONS 64
#define VK_VOLUMETRIC_MAX_VOLUMES 24
#define VK_VOLUMETRIC_MAX_LIGHTS 32
#define VK_VOLUMETRIC_QUERY_SLOTS 16
#define VK_VOLUMETRIC_TELEMETRY_COUNTERS 8

typedef struct {
	float invProj[16];
	float invView[16];
	float proj[16];
	float viewProj[16];
	float prevView[16];
	float prevViewProj[16];
	float viewOrigin[4];
	float sunDirection[4];
	float fogColor[4];
	float densityParams[4];
	float worldMin[4];
	float worldMax[4];
	float gridDim[4];
	float miscParams[4];
	float sliceParams[4];
	float phaseParams[4];
	float scatterParams[4];
	float noiseParams[4];
	float noiseScroll[4];
	float temporalParams[4];
	float qualityParams[4];
	float windParams[4];
	float volumeCounts[4];
	float passParams[4];
	float volumeBoundsMin[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float volumeBoundsMax[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float volumeColorDensity[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float volumeTypeParams[VK_VOLUMETRIC_MAX_VOLUMES][4];
	float lightPosRadius[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightColorType[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightDirAngle[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float lightExtra[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float sunShadowMatrix0[16];
	float shadowParams0[4];
	float shadowMapSize0[4];
	float localSpotShadowMatrix[VK_VOLUMETRIC_MAX_LIGHTS][16];
	float localPointShadowMatrix[VK_VOLUMETRIC_MAX_LIGHTS][6][16];
	float localShadowAtlasUv[VK_VOLUMETRIC_MAX_LIGHTS][4];
	float localSpotShadowMapSize[4];
	float localPointShadowMapSize[4];
	float fluidParams0[4];
	float fluidParams1[4];
	float fluidParams2[4];
	float fluidWorldMap[4];
	float fluidEmitters[16][4];
	float fluidEmitterData[16][4];
	float fluidEmitterCount[4];
	float telemetryParams0[4];
	float telemetryParams1[4];
} volumetric_params_t;

void vk_update_volumetric_params( void );

_Static_assert( ( sizeof( volumetric_params_t ) % 16 ) == 0, "volumetric_params_t must be 16-byte aligned in size" );
#define VK_VOLUMETRIC_ASSERT_ALIGNED16(member) _Static_assert( ( offsetof( volumetric_params_t, member ) % 16 ) == 0, "volumetric_params_t::" #member " must be 16-byte aligned" )
VK_VOLUMETRIC_ASSERT_ALIGNED16( invProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( invView );
VK_VOLUMETRIC_ASSERT_ALIGNED16( proj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( viewProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( prevView );
VK_VOLUMETRIC_ASSERT_ALIGNED16( prevViewProj );
VK_VOLUMETRIC_ASSERT_ALIGNED16( viewOrigin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sunDirection );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fogColor );
VK_VOLUMETRIC_ASSERT_ALIGNED16( densityParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( worldMin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( worldMax );
VK_VOLUMETRIC_ASSERT_ALIGNED16( gridDim );
VK_VOLUMETRIC_ASSERT_ALIGNED16( miscParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sliceParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( phaseParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( scatterParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( noiseParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( noiseScroll );
VK_VOLUMETRIC_ASSERT_ALIGNED16( temporalParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( qualityParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( windParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeCounts );
VK_VOLUMETRIC_ASSERT_ALIGNED16( passParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeBoundsMin );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeBoundsMax );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeColorDensity );
VK_VOLUMETRIC_ASSERT_ALIGNED16( volumeTypeParams );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightPosRadius );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightColorType );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightDirAngle );
VK_VOLUMETRIC_ASSERT_ALIGNED16( lightExtra );
VK_VOLUMETRIC_ASSERT_ALIGNED16( sunShadowMatrix0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( shadowParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( shadowMapSize0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localSpotShadowMatrix );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localPointShadowMatrix );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localShadowAtlasUv );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localSpotShadowMapSize );
VK_VOLUMETRIC_ASSERT_ALIGNED16( localPointShadowMapSize );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams1 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidParams2 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidWorldMap );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidEmitters );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidEmitterData );
VK_VOLUMETRIC_ASSERT_ALIGNED16( fluidEmitterCount );
VK_VOLUMETRIC_ASSERT_ALIGNED16( telemetryParams0 );
VK_VOLUMETRIC_ASSERT_ALIGNED16( telemetryParams1 );
#undef VK_VOLUMETRIC_ASSERT_ALIGNED16

#endif
