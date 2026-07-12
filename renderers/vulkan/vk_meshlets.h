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
void R_Meshlets_InvalidateCache( void );

/* Bake meshlets from position list + triangle indexes; returns count written to out[]. */
int R_Meshlets_Bake( const vec3_t *positions, int numVerts, const int *indexes, int numIndexes,
	meshlet_t *out, int maxOut );

/* Cache local-space bake keyed by surface (or other stable pointer). */
int R_Meshlets_CacheLocal( const void *key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes );
int R_Meshlets_Lookup( const void *key, const meshlet_t **outMeshlets );

/* Frustum cull against viewParms.frustum planes; returns visible count. */
int R_Meshlets_CullViewFrustum( const meshlet_t *meshlets, int count, int *visible, int maxVisible );

/* Transform local AABBs by entity pose, then frustum cull. */
int R_Meshlets_CullViewFrustumXform( const meshlet_t *meshlets, int count,
	const float entityAxis[3][3], const vec3_t entityOrigin, int *visible, int maxVisible );

#endif
