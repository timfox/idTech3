#include "renderer_abstraction.h"

// Implement C linkage wrappers so that C code can link against the C++-defined functions
extern "C" {
void PathTracer_RenderSample_wrapper(vec3_t result, const vec3_t origin, const vec3_t direction) {
    PathTracer_RenderSample(result, origin, direction);
}

void RTX_RenderScene_wrapper(const refdef_t *fd) {
    RTX_RenderScene(fd);
}
}
