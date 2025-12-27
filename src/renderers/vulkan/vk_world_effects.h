/*
=============================================================================
World Effects System (Vulkan)
Environmental effects like wind/weather/dust orchestration.
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

void vk_world_effects_init(void);
void vk_world_effects_shutdown(void);
void vk_world_effects_update(void);
void vk_world_effects_render(void);

extern cvar_t *r_worldEffects;
extern cvar_t *r_worldEffectsWind;
extern cvar_t *r_worldEffectsWindStrength;
extern cvar_t *r_worldEffectsWindFrequency;
extern cvar_t *r_worldEffectsWeather; /* 0=off, 1=rain, 2=snow (placeholder) */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // USE_VULKAN

