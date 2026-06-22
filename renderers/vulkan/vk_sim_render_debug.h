#ifndef VK_SIM_RENDER_DEBUG_H
#define VK_SIM_RENDER_DEBUG_H

#include "tr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int profile;
	int msaaSamples;
	int tonemapMode;
	qboolean fxaaActive;
	qboolean smaaActive;
	qboolean bloomActive;
	qboolean fogActive;
	qboolean fogAccurate;
	int fogIntegration;
	int fogSteps;
	int fogQuality;
	qboolean perfTimestamps;
	float fogTotalMs;
	float fogFluidMs;
	float fogClearMs;
	float fogGlobalMs;
	float fogVolumeMs;
	float fogSunMs;
	float fogLocalMs;
	float fogTemporalMs;
	float fogCompositeMs;
} vk_sim_render_debug_stats_t;

qboolean vk_volumetric_perf_wanted( void );
void VK_SimRenderDebugFillStats( vk_sim_render_debug_stats_t *out );
void VK_SimRenderDebugFrameEnd( void );
void VK_SimRenderDebugStartupLog( void );

#ifdef __cplusplus
}
#endif

#endif /* VK_SIM_RENDER_DEBUG_H */
