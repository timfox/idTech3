// Tiny usage example: dispatch both render paths via RendererAbstraction
// in a non-GPU environment. This demonstrates wiring and a replacement backend.
#include <stdio.h>
#include "renderers/renderer_abstraction.h"

// Mock backends
static void mock_render_sample(vec3_t result, const vec3_t origin, const vec3_t direction) {
    (void)result; (void)origin; (void)direction;
    printf("mock_render_sample invoked\n");
}

static void mock_render_scene(const refdef_t *fd) {
    (void)fd;
    printf("mock_render_scene invoked\n");
}

int main(void) {
    RendererAbstraction ra;
    ra.renderSample = mock_render_sample;
    ra.renderScene = mock_render_scene;

    vec3_t dummyRes = {0};
    vec3_t origin = {0.0f, 0.0f, 0.0f};
    vec3_t dir = {0.0f, 0.0f, -1.0f};
    refdef_t dummyFd;

    ra.renderSample(dummyRes, origin, dir);
    ra.renderScene(&dummyFd);

    printf("RendererAbstraction usage mock completed\n");
    return 0;
}

