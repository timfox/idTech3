// Verifies that the RendererAbstraction wrappers dispatch to the expected mock functions
// by providing mock implementations for PathTracer_RenderSample and RTX_RenderScene
// and invoking the wrappers directly.

#include <assert.h>
#include <stdio.h>

// Forward declare types used by the wrappers
typedef float vec3_t[3];
typedef struct { int dummy; } refdef_t;

// Prototypes for wrapper entry points (from header)
// The header provides these under C linkage when included in C++.
void PathTracer_RenderSample_wrapper(vec3_t result, const vec3_t origin, const vec3_t direction);
void RTX_RenderScene_wrapper(const refdef_t *fd);
// For address-level checks, include the RendererAbstraction header to get the typedefs
#include "renderers/renderer_abstraction.h"

// Mock counters
static int g_mock_pathTracer_calls = 0;
static int g_mock_rtx_calls = 0;

// Mock implementations
void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction) {
    (void)origin; (void)direction;
    g_mock_pathTracer_calls++;
    // Populate result to demonstrate a deterministic side-effect
    result[0] = 1.0f; result[1] = 2.0f; result[2] = 3.0f;
}

void RTX_RenderScene(const refdef_t *fd) {
    (void)fd;
    g_mock_rtx_calls++;
}

int main(int argc, char **argv) {
    // Support optional verbose-mode to print actual pointer values for inspection
    bool verboseWrappers = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose-wrappers") == 0) {
            verboseWrappers = true;
            break;
        }
    }
    // Reset counters
    g_mock_pathTracer_calls = 0;
    g_mock_rtx_calls = 0;

    // Exercise wrappers
    vec3_t out = {0.0f, 0.0f, 0.0f};
    vec3_t origin = {0.0f, 0.0f, 0.0f};
    vec3_t dir = {0.0f, 0.0f, -1.0f};
    refdef_t fd = {};

    PathTracer_RenderSample_wrapper(out, origin, dir);
    RTX_RenderScene_wrapper(&fd);

    // Assertions: both wrappers should dispatch to their mocks exactly once
    assert(g_mock_pathTracer_calls == 1);
    assert(g_mock_rtx_calls == 1);

    // Optional sanity: the wrapper should have filled 'out' with the mock values
    assert(out[0] == 1.0f && out[1] == 2.0f && out[2] == 3.0f);

    printf("RendererAbstraction wrappers dispatch asserted successfully\\n");

    if (verboseWrappers) {
        // Print actual function addresses for manual inspection
        printf("PathTracer_RenderSample_wrapper address: %p\\n", (void*)PathTracer_RenderSample_wrapper);
        printf("RTX_RenderScene_wrapper address: %p\\n", (void*)RTX_RenderScene_wrapper);
        // Print the addresses recorded in the default abstraction mapping
        RendererAbstraction ra = get_default_renderer_abstraction();
        printf("ra.renderSample pointer: %p\\n", (void*)ra.renderSample);
        printf("ra.renderScene pointer: %p\\n", (void*)ra.renderScene);
        // Also print the expected addresses for comparison
        printf("Expected sample wrapper: %p\\n", (void*)PathTracer_RenderSample_wrapper);
        printf("Expected scene wrapper: %p\\n", (void*)RTX_RenderScene_wrapper);
    }

    // Address-level checks: verify that the wrappers have the expected addresses
    using RenderSampleFunc = void (*)(vec3_t, const vec3_t, const vec3_t);
    using RenderSceneFunc = void (*)(const refdef_t *);
    RenderSampleFunc expectedSample = PathTracer_RenderSample_wrapper;
    RenderSceneFunc expectedScene = RTX_RenderScene_wrapper;
    // From the RendererAbstraction default wiring, renderSample should point to the PathTracer wrapper
    RendererAbstraction ra = get_default_renderer_abstraction();
    assert(ra.renderSample == (RenderSampleFunc)expectedSample);
    assert(ra.renderScene == (RenderSceneFunc)expectedScene);
    return 0;
}

