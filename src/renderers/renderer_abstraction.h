#pragma once

#include "renderercommon/tr_public.h"
#include "renderercommon/tr_types.h"

// Public function pointers to render via the active backend
typedef void (*RenderSampleFunc)(vec3_t result, const vec3_t origin, const vec3_t direction);
typedef void (*RenderSceneFunc)(const refdef_t *fd);

// Lightweight abstraction over rendering backends
typedef struct RendererAbstraction {
    RenderSampleFunc renderSample;
    RenderSceneFunc renderScene;
} RendererAbstraction;

// Prototypes for the backend entry points (C-callable)
void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction);
void RTX_RenderScene(const refdef_t *fd);

// C linkage wrappers (to ensure clean linkage with C++-defined symbols)
#ifdef __cplusplus
extern "C" {
#endif
void PathTracer_RenderSample_wrapper(vec3_t result, const vec3_t origin, const vec3_t direction);
void RTX_RenderScene_wrapper(const refdef_t *fd);
#ifdef __cplusplus
}
#endif

// Factory: return a default RendererAbstraction mapping
// PathTracer_RenderSample -> renderSample
// RTX_RenderScene       -> renderScene
static inline RendererAbstraction get_default_renderer_abstraction(void) {
    RendererAbstraction ra;
    ra.renderSample = PathTracer_RenderSample_wrapper;
    ra.renderScene = RTX_RenderScene_wrapper;
    return ra;
}
