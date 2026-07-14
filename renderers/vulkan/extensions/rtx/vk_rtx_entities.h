#ifndef VK_RTX_ENTITIES_H
#define VK_RTX_ENTITIES_H

#ifdef USE_VULKAN_RTX

#include "tr_types.h"

typedef struct {
	uint32_t entityCount;
	uint32_t vertexCount;
	uint32_t primitiveCount;
	uint32_t meshEntityCount;
	uint32_t meshMd3Count;
	uint32_t meshIqmCount;
	uint32_t meshGltfCount;
	uint32_t meshMdrCount;
	uint32_t proxyEntityCount;
	uint32_t proxyNonMeshCount;    /* brush/unknown → AABB */
	uint32_t proxySkinnedCount;    /* reserved (kept for status ABI) */
	uint32_t proxyMd3FailCount;    /* MOD_MESH but pack_md3 failed → AABB */
	uint32_t proxyIqmFailCount;
	uint32_t proxyGltfFailCount;
	uint32_t proxyMdrFailCount;
} vkRtxEntityPackStats_t;

/*
 * Pack RT_MODEL entities into world-space vertex/index buffers for a single entity BLAS.
 * Prefers MD3 LOD0, IQM (CPU-skinned), glTF (static/CPU-skinned), MDR LOD0 (CPU-skinned).
 * AABB proxy for unknown types and pack failures.
 * Optional albedoRgb/normalNxyz: per-primitive float[prim*3] (same layout as world SSBOs).
 */
uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices,
	float *albedoRgb, float *normalNxyz, vkRtxEntityPackStats_t *stats );

#else /* !USE_VULKAN_RTX */

typedef struct {
	uint32_t entityCount;
	uint32_t vertexCount;
	uint32_t primitiveCount;
	uint32_t meshEntityCount;
	uint32_t meshMd3Count;
	uint32_t meshIqmCount;
	uint32_t meshGltfCount;
	uint32_t meshMdrCount;
	uint32_t proxyEntityCount;
	uint32_t proxyNonMeshCount;
	uint32_t proxySkinnedCount;
	uint32_t proxyMd3FailCount;
	uint32_t proxyIqmFailCount;
	uint32_t proxyGltfFailCount;
	uint32_t proxyMdrFailCount;
} vkRtxEntityPackStats_t;

uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices,
	float *albedoRgb, float *normalNxyz, vkRtxEntityPackStats_t *stats );

#endif /* USE_VULKAN_RTX */

#endif /* VK_RTX_ENTITIES_H */
