#pragma once

/*
 * Reflection source hierarchy foundation.
 * See docs/REFLECTION_HIERARCHY.md
 */


#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {

typedef enum {
	VK_REFLECT_SRC_NONE = 0,
	VK_REFLECT_SRC_PLANAR,
	VK_REFLECT_SRC_SSR,
	VK_REFLECT_SRC_RAY,
	VK_REFLECT_SRC_PROBE,
	VK_REFLECT_SRC_SKY
} vkReflectSource_t;

#define VK_REFLECTION_SOURCE_NONE   VK_REFLECT_SRC_NONE
#define VK_REFLECTION_SOURCE_PLANAR VK_REFLECT_SRC_PLANAR
#define VK_REFLECTION_SOURCE_SSR    VK_REFLECT_SRC_SSR
#define VK_REFLECTION_SOURCE_RAY    VK_REFLECT_SRC_RAY
#define VK_REFLECTION_SOURCE_PROBE  VK_REFLECT_SRC_PROBE
#define VK_REFLECTION_SOURCE_SKY    VK_REFLECT_SRC_SKY

typedef struct {
	vkReflectSource_t source;
	float             weight;
	char              note[64];
} vkReflectionResult_t;

void vk_reflection_hierarchy_register( void );
void vk_reflection_hierarchy_begin_frame( void );
void vk_reflection_hierarchy_note( vkReflectSource_t source, float weight, const char *note );

#ifdef __cplusplus
}
#endif

#endif /* USE_VULKAN */
