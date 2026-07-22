#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define VK_INDIRECT_LIGHT_MAX_PROBES 256

typedef struct {
	vec3_t position;
	vec3_t irradiance;
	float radius;
	uint32_t generation;
	uint32_t flags;
} vkIrradianceProbe_t;

void vk_indirect_light_register( void );
void vk_indirect_light_begin_frame( void );
void vk_indirect_light_set_probe( uint32_t index, const vec3_t pos, const vec3_t irradiance,
	float radius, uint32_t flags );

#endif /* USE_VULKAN */
