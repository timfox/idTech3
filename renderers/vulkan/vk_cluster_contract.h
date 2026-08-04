#pragma once

/*
 * Clustered Hybrid M2 — authoritative CPU/GPU cluster contract.
 * See docs/CLUSTERED_LIGHTING.md and docs/RENDERER_PATH_OWNERSHIP.md.
 *
 * viewDepth = positive forward view-space distance.
 * zFar policy: effectiveFar = min(r_clusterZFar, camera_zFar), floored above zNear.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_CLUSTER_TILE_SIZE_X   16u
#define VK_CLUSTER_TILE_SIZE_Y   16u
#define VK_CLUSTER_HEADER_BYTES  8u
#define VK_CLUSTER_PARAMS_BYTES  56u /* 14 x uint32 / floats packed */

/* Flags for gpuClusterParams_t.flags */
#define VK_CLUSTER_FLAG_LOG_Z           1u
#define VK_CLUSTER_FLAG_COMPACT_LISTS   2u
#define VK_CLUSTER_FLAG_OVERFLOW        4u
#define VK_CLUSTER_FLAG_BUILD_FAILED    8u
#define VK_CLUSTER_FLAG_FALLBACK_LEGACY 16u

typedef struct gpuClusterHeader_s {
	uint32_t offset;
	uint32_t count;
} gpuClusterHeader_t;

typedef struct gpuClusterParams_s {
	uint32_t clusterCountX;
	uint32_t clusterCountY;
	uint32_t clusterCountZ;
	uint32_t tileSizeX;
	uint32_t tileSizeY;
	uint32_t lightIndexCapacity;
	float    zNear;
	float    zFar;
	float    zScale;
	float    zBias;
	uint32_t generation;
	uint32_t overflowCount;
	uint32_t flags;
	uint32_t reserved;
} gpuClusterParams_t;

#ifdef __cplusplus
static_assert( sizeof( gpuClusterHeader_t ) == 8, "gpuClusterHeader_t must be 8 bytes" );
static_assert( sizeof( gpuClusterParams_t ) == VK_CLUSTER_PARAMS_BYTES,
	"gpuClusterParams_t size must match the GLSL ClusterParams ABI" );
static_assert( sizeof( gpuClusterHeader_t ) == VK_CLUSTER_HEADER_BYTES,
	"VK_CLUSTER_HEADER_BYTES mismatch" );
#else
_Static_assert( sizeof( gpuClusterHeader_t ) == 8, "gpuClusterHeader_t must be 8 bytes" );
_Static_assert( sizeof( gpuClusterParams_t ) == VK_CLUSTER_PARAMS_BYTES,
	"gpuClusterParams_t size must match the GLSL ClusterParams ABI" );
_Static_assert( sizeof( gpuClusterHeader_t ) == VK_CLUSTER_HEADER_BYTES,
	"VK_CLUSTER_HEADER_BYTES mismatch" );
#endif

/* Derive log2 slice scale/bias so slices cover [zNear, zFar]. */
void Cluster_DeriveLogZScaleBias( float zNear, float zFar, uint32_t clusterCountZ,
	float *outScale, float *outBias );

/* Positive view-depth → slice index. zMode 0=linear, 1=log2 (production). */
uint32_t Cluster_ViewDepthToSlice( float viewDepth, uint32_t clusterCountZ, uint32_t zMode,
	float zNear, float zFar, float zScale, float zBias );

uint32_t Cluster_IndexFromPixelAndViewDepth( uint32_t pixelX, uint32_t pixelY, float viewDepth,
	const gpuClusterParams_t *params, uint32_t zMode );

uint32_t Cluster_IndexFromTileAndSlice( uint32_t tileX, uint32_t tileY, uint32_t slice,
	const gpuClusterParams_t *params );

void Cluster_SliceDepthRange( uint32_t slice, uint32_t clusterCountZ, uint32_t zMode,
	float zNear, float zFar, float zScale, float zBias,
	float *outNear, float *outFar );

uint32_t Cluster_LightOverlapsSlice( float lightNear, float lightFar,
	float sliceNear, float sliceFar );

void Cluster_LightSliceSpan( float lightNear, float lightFar, const gpuClusterParams_t *params,
	uint32_t zMode, uint32_t *outFirstSlice, uint32_t *outLastSlice );

uint32_t Cluster_TotalCount( const gpuClusterParams_t *params );

#ifdef __cplusplus
}
#endif
