#ifndef VK_RTX_ENTITIES_H
#define VK_RTX_ENTITIES_H

#ifdef USE_VULKAN_RTX

#include "tr_types.h"

/* Pack RT_MODEL refEntity proxy AABBs (12 tris each) into world-space vertex/index buffers. */
uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t *indices );

#endif /* USE_VULKAN_RTX */

#endif /* VK_RTX_ENTITIES_H */
