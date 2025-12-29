/*
===========================================================================
id Tech 3 - Path Tracer Header

Path tracing implementation for RTX renderer
===========================================================================
*/

#ifndef __VK_PATH_TRACER_H__
#define __VK_PATH_TRACER_H__

// Path tracer statistics
typedef struct {
    int total_rays;
    int total_bounces;
    float avg_bounces;
} path_tracer_stats_t;

// Path tracer API
void PathTracer_Init(void);
void PathTracer_Shutdown(void);
void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction);
void PathTracer_UpdateStatistics(void);
void PathTracer_GetStatistics(int *total_rays, int *total_bounces, float *avg_bounces);
void PathTracer_ResetStatistics(void);

#endif // __VK_PATH_TRACER_H__