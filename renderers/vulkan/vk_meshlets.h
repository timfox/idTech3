#ifndef VK_MESHLETS_H
#define VK_MESHLETS_H

#include "tr_local.h"

#define MESHLET_MAX_VERTS 64
#define MESHLET_MAX_TRIS  126
#define MESHLET_MAX_PER_SURFACE 512

typedef struct {
	vec3_t mins;
	vec3_t maxs;
	vec3_t coneAxis;   /* average face normal (local); length 0 = disabled */
	float  coneCutoff; /* cos(half-angle); < -1 = disabled */
	uint16_t firstIndex;
	uint16_t indexCount;
	uint16_t firstVert;
	uint16_t vertCount;
	uint32_t materialClass; /* 0 opaque, 1 alpha-test, 2 other */
} meshlet_t;

/* Host-side VkDrawIndexedIndirectCommand layout (5 x uint32). */
typedef struct {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t  vertexOffset;
	uint32_t firstInstance;
} meshlet_draw_cmd_t;

/* GPU meshlet ABI. Keep this 16-byte aligned; it is consumed by the portable
 * compute culler and by the optional mesh-shader path. */
typedef struct {
	float mins[3];
	uint32_t firstIndex;
	float maxs[3];
	uint32_t indexCount;
	float coneAxis[3];
	float coneCutoff;
	uint32_t surfaceIndex;
	uint32_t lodMinPixels;
	uint32_t lodMaxPixels;
	uint32_t streamState;
	uint32_t generation;
} meshlet_gpu_record_t;

/* Persistent per-surface index storage. The CPU tess index buffer remains a
 * correctness fallback, but GPU-generated commands address this arena. */
typedef struct {
	uint64_t key;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t generation;
	uint32_t streamState;
	qboolean resident;
} meshlet_surface_gpu_t;

void R_Meshlets_Init( void );
void R_Meshlets_Shutdown( void );
qboolean R_Meshlets_Active( void );
qboolean R_Meshlets_WantMdi( void );
void R_Meshlets_InvalidateCache( void );

/* Bake meshlets from position list + triangle indexes; returns count written to out[]. */
int R_Meshlets_Bake( const vec3_t *positions, int numVerts, const int *indexes, int numIndexes,
	meshlet_t *out, int maxOut );

/* Cache local-space bake keyed by stable uint64 (not transient CPU pointers). */
uint64_t R_Meshlets_StableKey( const char *modelName, const char *surfaceName, int surfaceIndex );
int R_Meshlets_CacheLocalKey( uint64_t key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes );

/* Register/copy a surface's indexes into persistent GPU-visible storage. */
qboolean R_Meshlets_RegisterPersistentIndexes( uint64_t key, const int *indexes,
	int numIndexes, meshlet_surface_gpu_t *out );
qboolean R_Meshlets_PersistentIndexBufferReady( void );
void *R_Meshlets_PersistentIndexBuffer( void );
uint32_t R_Meshlets_PersistentIndexCount( void );
int R_Meshlets_LookupKey( uint64_t key, const meshlet_t **outMeshlets );

/* Legacy pointer key — hashes address + generation; prefer StableKey / CacheLocalKey. */
int R_Meshlets_CacheLocal( const void *key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes );
int R_Meshlets_Lookup( const void *key, const meshlet_t **outMeshlets );

/* Frustum cull against viewParms.frustum planes; returns visible count. */
int R_Meshlets_CullViewFrustum( const meshlet_t *meshlets, int count, int *visible, int maxVisible );

/* Transform local AABBs by entity pose, then frustum cull. */
int R_Meshlets_CullViewFrustumXform( const meshlet_t *meshlets, int count,
	const float entityAxis[3][3], const vec3_t entityOrigin, int *visible, int maxVisible );

/* Pack MDI draw args from visible meshlet indices (r_meshletsMdi). vertexOffset applied to each cmd. */
int R_Meshlets_PackIndirect( const meshlet_t *meshlets, const int *visible, int visibleCount,
	meshlet_draw_cmd_t *outCmds, int maxCmds, int32_t vertexOffset );

/* Append remapped indexes for visible meshlets into tess (compact draw). Returns index count added.
 * When r_meshletsMdiDraw 1, also enqueues tess-relative indirect cmds for GPU MDI flush. */
int R_Meshlets_AppendVisibleIndexes( md3Surface_t *surface, int vertexBase,
	const float entityAxis[3][3], const vec3_t entityOrigin );

qboolean R_Meshlets_WantCompact( void );
qboolean R_Meshlets_WantMdiDraw( void );
/* Clear per-tess-batch MDI cmd queue (call from RB_BeginSurface). */
void R_Meshlets_BeginSurface( void );
/* If MDI draw pending: upload cmds + vkCmdDrawIndexedIndirect. Returns qtrue if drew. */
qboolean R_Meshlets_TryDrawIndirect( void );

#endif
