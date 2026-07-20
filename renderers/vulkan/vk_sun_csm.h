/*
===========================================================================
Cascaded sun shadows (Raster Ultra 1.1).
Atlas layout: N cascades in a sqrt-ceil grid (1→1x1, 2–4→2x2).
===========================================================================
*/

#ifndef VK_SUN_CSM_H
#define VK_SUN_CSM_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VK_SUN_CSM_MAX 4

void VK_SunCSM_Init( void );
void VK_SunCSM_Shutdown( void );

int VK_SunCSM_CascadeCount( void ); /* 1..4; 1 = legacy single map */
float VK_SunCSM_SplitLambda( void );
float VK_SunCSM_MaxDistance( void );
float VK_SunCSM_CascadeBlend( void );
qboolean VK_SunCSM_Stable( void );
int VK_SunCSM_Debug( void );

/* Practical (PSSM) split distances in view-depth [near, far]. splitsOut[i] = far of cascade i. */
void VK_SunCSM_ComputeSplits( float nearPlane, float farPlane, int cascadeCount,
	float lambda, float *splitsOut );

/* Atlas tile (tx,ty) for cascade index; tileSize = shadowMapSize / grid. */
void VK_SunCSM_AtlasTile( int cascade, int cascadeCount, int mapSize,
	int *outX, int *outY, int *outTileSize, int *outAtlasSize );

#ifdef __cplusplus
}
#endif

#endif /* VK_SUN_CSM_H */
