/*
===========================================================================
Vulkan Renderer - q2rtx-style Implementation
===========================================================================
*/

#include "tr_local.h"
#include <dlfcn.h>

#include "vk.h"

// Engine interface - will be set by GetRefAPI
#ifdef USE_RENDERER_DLOPEN
// ri is now defined in tr_services.c
#else
static refimport_t ri;
#endif

// Stub cvars for compatibility (Vulkan renderer doesn't use these directly)
cvar_t *r_dynamiclight = NULL;
cvar_t *r_showImages = NULL;
cvar_t *r_zproj = NULL;
cvar_t *r_stereoSeparation = NULL;
cvar_t *r_znear = NULL;
cvar_t *r_drawworld = NULL;
cvar_t *r_lockpvs = NULL;
cvar_t *r_showcluster = NULL;
cvar_t *r_novis = NULL;
cvar_t *r_clear = NULL;
cvar_t *r_clearcoat = NULL;
cvar_t *r_vk_debug2D = NULL;
cvar_t *r_debugSurface = NULL;

// Define gls and glState since they are declared extern in tr_local.h
// Stub global state for compatibility
glstatic_t gls = {0};
glstate_t glState = {0};

// Stub cvars for compatibility
cvar_t *r_skipBackEnd = NULL;
cvar_t *r_finish = NULL;
cvar_t *r_teleporterFlash = NULL;
cvar_t *r_vk_debugClearColor = NULL;
cvar_t *r_vk_debugUiOnly = NULL;
cvar_t *r_drawSun = NULL;
cvar_t *r_flares = NULL;
cvar_t *r_flareFade = NULL;
cvar_t *r_flareSize = NULL;
cvar_t *r_flareCoeff = NULL;

// Stub functions for missing Vulkan functionality
void vk_draw_dot(uint32_t storage_offset) {
    Q_UNUSED(storage_offset);
}

// Forward declarations - these functions are implemented in other files
extern void RB_TakeScreenshot(int x, int y, int width, int height, const char *fileName);
extern void RB_TakeScreenshotJPEG(int x, int y, int width, int height, const char *fileName);
extern void RB_TakeScreenshotBMP(int x, int y, int width, int height, const char *fileName, int clipboard);
extern image_t *R_CreateImage(const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags, int format, uint32_t type);
extern const void *RB_TakeVideoFrameCmd(const void *data);
extern skin_t *R_GetSkinByHandle(qhandle_t handle);

// ============================================================================
// Vulkan-specific functions
// ============================================================================

// Note: Vulkan initialization is handled by the engine, not by the renderer

static void vk_shutdown_local(void) {
    ri.Printf(PRINT_ALL, "Vulkan Renderer: Shutting down...\n");

    // Clean up Vulkan resources if device is valid and not lost
    if (vk.device != VK_NULL_HANDLE && vk.device != (VkDevice)0x20000000 && !vk.device_lost) {
        // RTX-specific resources are cleaned up by RTX_Shutdown() which is called
        // before this function. This ensures proper cleanup order:
        // 1. RTX_Shutdown() -> vk_rt_shutdown(), VK_ComputeRT_Shutdown()
        // 2. This function -> general Vulkan cleanup
        // 3. Main Vulkan shutdown -> device destruction
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Resources cleaned up by RTX_Shutdown()\n");
    } else {
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Skipping cleanup (device lost or invalid)\n");
    }
}

// ============================================================================
// Renderer Interface Implementation
// ============================================================================

void RE_Shutdown(refShutdownCode_t code) {
    ri.Printf(PRINT_ALL, "Vulkan Renderer: Shutdown (%i)\n", code);
    vk_shutdown_local();
}

void RE_BeginRegistration(glconfig_t *glconfigOut) {

    // Initialize scene management if Vulkan is active
    if (vk.active) {
        vk.scene.initialized = qtrue;
        vk.scene.entityCount = 0;
        vk.scene.polygonCount = 0;
    }

    // Fill in Vulkan config
    Com_Memset(&glConfig, 0, sizeof(glConfig));
    glConfig.colorBits = 32;
    glConfig.depthBits = 24;
    glConfig.stencilBits = 8;
    glConfig.deviceSupportsGamma = qtrue;
    glConfig.textureCompression = TC_S3TC;
    glConfig.textureEnvAddAvailable = qtrue;
    glConfig.maxTextureSize = 4096;
    glConfig.displayFrequency = 60;
    glConfig.isFullscreen = qfalse;
    glConfig.stereoEnabled = qfalse;
    glConfig.vidWidth = 1024;
    glConfig.vidHeight = 768;

    *glconfigOut = glConfig;
}

qhandle_t RE_RegisterModel(const char *name) {
    // Stub - return invalid handle
    Q_UNUSED(name);
    return 0;
}

qhandle_t RE_RegisterSkin(const char *name) {
    // Stub - return invalid handle
    Q_UNUSED(name);
    return 0;
}

qhandle_t RE_RegisterShader(const char *name) {
    // Register shader with Vulkan backend
    return vk_register_shader(name);
}

qhandle_t RE_RegisterShaderNoMip(const char *name) {
    // Register shader (no mip) with Vulkan backend
    return vk_register_shader(name);
}

void RE_LoadWorldMap(const char *name) {
    ri.Printf(PRINT_ALL, "Vulkan Renderer: LoadWorldMap %s\n", name);
}

void RE_SetWorldVisData(const byte *vis) {
    // Stub
    Q_UNUSED(vis);
}

void RE_EndRegistration(void) {
    ri.Printf(PRINT_ALL, "Vulkan Renderer: EndRegistration\n");
}

void RE_BeginFrame(stereoFrame_t stereoFrame) {
    // Stub
    Q_UNUSED(stereoFrame);

    vk_begin_frame();
}

void RE_EndFrame(int *frontEndMsec, int *backEndMsec) {
    vk_end_frame();

    // Stub timing
    if (frontEndMsec) *frontEndMsec = 0;
    if (backEndMsec) *backEndMsec = 0;
}

int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir) {
    // Return basic lighting
    VectorSet(ambientLight, 0.5f, 0.5f, 0.5f);
    VectorSet(directedLight, 0.5f, 0.5f, 0.5f);
    VectorSet(lightDir, 0, 0, -1);
    Q_UNUSED(point);
    return 0;
}

void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
    // Stub
    Q_UNUSED(org); Q_UNUSED(intensity); Q_UNUSED(r); Q_UNUSED(g); Q_UNUSED(b);
}

void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
    // Stub
    Q_UNUSED(org); Q_UNUSED(intensity); Q_UNUSED(r); Q_UNUSED(g); Q_UNUSED(b);
}

void RE_RenderScene(const refdef_t *fd) {
    // Render the 3D scene
    if (vk.active) {
        vk_render_scene(fd);
    }
    // If Vulkan not active, rendering is skipped (no fallback needed for basic operation)
}

void RE_ClearScene(void) {
    // Clear the scene
    if (vk.active) {
        vk_clear_scene();
    }
}

void RE_AddRefEntityToScene(const refEntity_t *re, qboolean intShaderTime) {
    // Add entity to scene
    if (vk.active && re) {
        vk_add_entity(re, intShaderTime);
    }
}

void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num) {
    // Add polygon to scene
    if (vk.active && verts && numVerts > 0) {
        vk_add_polygon(hShader, numVerts, verts, num);
    }
}

void RE_SetColor(const float *rgba) {
    // Set current rendering color
    if (vk.active) {
        vk_set_color(rgba);
    }
}

void RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    // Draw 2D stretched image
    if (vk.active) {
        vk_draw_stretch_pic(x, y, w, h, s1, t1, s2, t2, hShader);
    }
}

void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty) {
    // Draw raw image data
    if (vk.active && data) {
        vk_draw_stretch_raw(x, y, w, h, cols, rows, data, client, dirty);
    }
}

// Forward declarations
extern void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
extern void RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);
extern qboolean RE_GetEntityToken(char *buffer, int size);

// These functions are implemented in tr_backend.c, tr_shader.c, and tr_bsp.c respectively
// They are declared here to satisfy the renderer interface but delegate to actual implementations

void RE_TakeVideoFrame(int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg) {
    // Stub
    Q_UNUSED(width); Q_UNUSED(height); Q_UNUSED(captureBuffer);
    Q_UNUSED(encodeBuffer); Q_UNUSED(motionJpeg);
}

// ============================================================================
// GetRefAPI - Main renderer entry point
// ============================================================================

Q_EXPORT refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp) {
    // Debug output at the very beginning
    fprintf(stderr, "VULKAN_RENDERER: GetRefAPI ENTRY POINT CALLED (apiVersion=%d, rimp=%p)\n", apiVersion, (void*)rimp);

    fprintf(stderr, "VULKAN_DEBUG: Initializing refimport_t\n");

    ri = *rimp;
    fprintf(stderr, "VULKAN_DEBUG: refimport_t assigned, setting up renderer\n");

    Com_Memset(&re, 0, sizeof(re));

    fprintf(stderr, "VULKAN_DEBUG: Checking API version - got %d, expected %d\n", apiVersion, REF_API_VERSION);
    if (apiVersion != REF_API_VERSION) {
        fprintf(stderr, "VULKAN_DEBUG: Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion);
        return NULL;
    }
    fprintf(stderr, "VULKAN_DEBUG: API version check passed\n");

    // Core renderer functions
    re.Shutdown = RE_Shutdown;
    re.BeginRegistration = RE_BeginRegistration;
    re.RegisterModel = RE_RegisterModel;
    re.RegisterSkin = RE_RegisterSkin;
    re.RegisterShader = RE_RegisterShader;
    re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
    re.LoadWorld = RE_LoadWorldMap;
    re.SetWorldVisData = RE_SetWorldVisData;
    re.EndRegistration = RE_EndRegistration;
    re.ClearScene = RE_ClearScene;
    re.AddRefEntityToScene = RE_AddRefEntityToScene;
    re.AddPolyToScene = RE_AddPolyToScene;
    re.LightForPoint = R_LightForPoint;
    re.AddLightToScene = RE_AddLightToScene;
    re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
    re.RenderScene = RE_RenderScene;
    re.SetColor = RE_SetColor;
    re.DrawStretchPic = RE_StretchPic;
    re.DrawStretchRaw = RE_StretchRaw;
    re.UploadCinematic = RE_UploadCinematic;
    re.RemapShader = RE_RemapShader;
    re.GetEntityToken = RE_GetEntityToken;
    re.TakeVideoFrame = RE_TakeVideoFrame;
    re.BeginFrame = RE_BeginFrame;
    re.EndFrame = RE_EndFrame;

    fprintf(stderr, "VULKAN_DEBUG: API interface initialized successfully\n");
    fprintf(stderr, "VULKAN_DEBUG: GetRefAPI returning valid refexport_t\n");

    return &re;
}
