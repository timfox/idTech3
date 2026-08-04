#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

/* Shared admission policy for selective RT sidecars. Primary visibility stays
 * raster-owned; these budgets only decide secondary ray work. */
typedef struct {
	uint32_t shadowBudget;
	uint32_t reflectionBudget;
	uint32_t heroObjectBudget;
	uint32_t raysPerPixelBudget;
	uint32_t shadowsAdmitted;
	uint32_t reflectionsAdmitted;
	uint32_t heroObjectsAdmitted;
	uint32_t budgetRejects;
} vkSelectiveRtBudget_t;

void vk_srt_init( void );
void vk_srt_shutdown( void );
void vk_srt_frame_begin( void );
qboolean vk_srt_admit_shadow( void );
qboolean vk_srt_admit_reflection( void );
qboolean vk_srt_admit_hero_object( void );
const vkSelectiveRtBudget_t *vk_srt_budget( void );
void vk_srt_status_f( void );

#endif
