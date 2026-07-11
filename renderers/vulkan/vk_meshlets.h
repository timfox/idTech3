#ifndef VK_MESHLETS_H
#define VK_MESHLETS_H

#include "tr_local.h"

#define MESHLET_MAX_VERTS 64
#define MESHLET_MAX_TRIS  126
#define MESHLET_MAX_PER_SURFACE 512

typedef struct {
	vec3_t mins;
	vec3_t maxs;
	uint16_t firstIndex;
	uint16_t indexCount;
	uint16_t firstVert;
	uint16_t vertCount;
} meshlet_t;

void R_Meshlets_Init( void );
void R_Meshlets_Shutdown( void );
qboolean R_Meshlets_Active( void );

/* Bake meshlets from position list + triangle indexes; returns count written to out[]. */
int R_Meshlets_Bake( const vec3_t *positions, int numVerts, const int *indexes, int numIndexes,
	meshlet_t *out, int maxOut );

/* Frustum cull against viewParms.frustum planes; returns visible count. */
int R_Meshlets_CullViewFrustum( const meshlet_t *meshlets, int count, int *visible, int maxVisible );

#endif
