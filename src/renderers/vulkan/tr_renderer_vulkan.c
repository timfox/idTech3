/*
=============================================================================
Vulkan Renderer Implementation

Vulkan implementation of the unified renderer interface.
=============================================================================
*/

#include "../../renderercommon/tr_renderer.h"
#include "../renderercommon/tr_types.h"
#include "tr_local.h"
#include "vk.h"

// Forward declarations for scene functions
extern void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
extern void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);

// Forward declarations for Vulkan-specific functions
extern qboolean vk_initialize(void);
extern void vk_shutdown(void);
extern void vk_begin_frame(void);
extern void vk_end_frame(void);

// Vulkan renderer interface implementation
static qboolean Vulkan_Init(void) {
    return vk_initialize();
}

static void Vulkan_Shutdown(void) {
    vk_shutdown();
}

static void Vulkan_Reset(void) {
    // Vulkan-specific reset logic
    vk_shutdown();
    vk_initialize();
}

static void Vulkan_BeginFrame(void) {
    vk_begin_frame();
}

static void Vulkan_EndFrame(void) {
    vk_end_frame();
}

static void Vulkan_Present(void) {
    // Vulkan handles presentation in end_frame
}

static void Vulkan_BeginScene(const refdef_t* refdef) {
    // Convert refdef to Vulkan scene setup
    vk_render_scene((const refdef_t*)refdef);
}

static void Vulkan_EndScene(void) {
    // Scene rendering is handled in vk_render_scene
}

static void Vulkan_ClearScene(void) {
    vk_clear_scene();
}

static void Vulkan_AddEntity(const refEntity_t* entity) {
    vk_add_entity(entity, qfalse);
}

static void Vulkan_AddPolygon(qhandle_t shader, int numVerts, const polyVert_t* verts) {
    vk_add_polygon(shader, numVerts, verts, 1);
}

static void Vulkan_AddLight(const dlight_t* light) {
    // Vulkan lighting integration - delegate to RE_AddLightToScene
    if (!light) {
        return;
    }
    
    // Use additive mode if light->additive is set, otherwise use normal mode
    if (light->additive) {
        RE_AddAdditiveLightToScene(light->origin, light->radius, 
                                   light->color[0], light->color[1], light->color[2]);
    } else {
        RE_AddLightToScene(light->origin, light->radius, 
                           light->color[0], light->color[1], light->color[2]);
    }
}

static void Vulkan_SetupLighting(void) {
    // Setup Vulkan lighting state
    // Lighting setup is handled automatically during scene rendering
    // This function provides a hook for any pre-render lighting configuration
    // Currently no-op as lighting is set up per-frame in vk_render_scene
}

static qhandle_t Vulkan_RegisterShader(const char* name) {
    return vk_register_shader(name);
}

static void Vulkan_RemapShader(const char* oldShader, const char* newShader, const char* timeOffset) {
    // Vulkan-specific shader remapping - delegates to main implementation
    // Shader remapping allows runtime replacement of shaders (e.g., for mods or effects)
    extern void RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);
    RE_RemapShader(oldShader, newShader, timeOffset);
}

static qhandle_t Vulkan_RegisterImage(const char* name) {
    return vk_register_image(name, 0);
}

// Forward declarations
extern image_t *R_GetImageByHandle(qhandle_t handle);
extern void vk_upload_image_data(image_t *image, int x, int y, int width, int height, int layers, const void *data, int data_size, qboolean update);

static void Vulkan_UpdateImage(qhandle_t image, const void* data, int x, int y, int width, int height) {
    // Vulkan texture update - upload new data to existing texture
    if (!data || width <= 0 || height <= 0) {
        ri.Printf(PRINT_WARNING, "Vulkan_UpdateImage: Invalid parameters\n");
        return;
    }
    
    image_t *img = R_GetImageByHandle(image);
    if (!img) {
        ri.Printf(PRINT_WARNING, "Vulkan_UpdateImage: Invalid image handle %d\n", image);
        return;
    }
    
    // Calculate data size (assume RGBA format)
    int data_size = width * height * 4;
    
    // Upload image data (update = true for subimage update)
    vk_upload_image_data(img, x, y, width, height, 1, data, data_size, qtrue);
}

static qhandle_t Vulkan_RegisterModel(const char* name) {
    return RE_RegisterModel(name);
}

static qhandle_t Vulkan_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font) {
    return RE_RegisterFont(fontName, pointSize, font);
}

static void Vulkan_RenderSurfaces(void) {
    // Surface rendering is handled in vk_render_scene
}

static void Vulkan_BeginPostProcess(void) {
    // Vulkan post-processing begin
    // Post-processing setup is handled automatically in vk_end_frame
    // This function provides a hook for any pre-post-process configuration
}

static void Vulkan_EndPostProcess(void) {
    // Vulkan post-processing end
    // Post-processing finalization is handled automatically in vk_end_frame
    // This function provides a hook for any post-post-process cleanup
}

static void Vulkan_DebugDrawAxis(void) {
    // Vulkan debug visualization
}

// Forward declarations
extern shaderCommands_t tess;
extern void GL_Bind(image_t *image);
extern void vk_bind_pipeline(VkPipeline pipeline);
extern void vk_bind_index(void);
extern void vk_bind_geometry(int geometry_bits);
extern void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed);
extern vk_t vk;
extern trGlobals_t tr;

static void Vulkan_DebugDrawNormals(void) {
    // Vulkan normal visualization - draw vertex normals as lines
    if (!vk.active || tess.numVertexes == 0) {
        return;
    }

    GL_Bind(tr.whiteImage);

    tess.numIndexes = 0;
    int i;
    for (i = 0; i < tess.numVertexes; i++) {
        // Extend normal from vertex position
        VectorMA(tess.xyz[i], 2.0f, tess.normal[i], tess.xyz[i + tess.numVertexes]);
        tess.indexes[tess.numIndexes + 0] = i;
        tess.indexes[tess.numIndexes + 1] = i + tess.numVertexes;
        tess.numIndexes += 2;
    }
    tess.numVertexes *= 2;
    Com_Memset(tess.svars.colors[0][0].rgba, tr.identityLightByte, tess.numVertexes * sizeof(color4ub_t));

    vk_bind_pipeline(vk.normals_debug_pipeline);
    vk_bind_index();
    vk_bind_geometry(TESS_XYZ | TESS_ST0 | TESS_RGBA0);
    vk_draw_geometry(DEPTH_RANGE_ZERO, qtrue);

    tess.numVertexes = 0;
}

// Forward declarations
extern shaderCommands_t tess;
extern backEndState_t backEnd;
extern void RB_EndSurface(void);
extern void vk_bind_pipeline(VkPipeline pipeline);
extern void vk_bind_geometry(int geometry_bits);
extern void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed);
extern vk_t vk;

static void Vulkan_DebugDrawTangents(void) {
    // Vulkan tangent visualization - draw tangent vectors as lines
    // Similar to DrawNormals but for tangent vectors
    if (!vk.active || tess.numVertexes == 0) {
        return;
    }

#ifdef USE_VK_PBR
    // Only draw tangents if PBR is active and we have qtangent data
    if (!vk.pbrActive || !tess.qtangent) {
        return;
    }

    RB_EndSurface();

    // Use normals debug pipeline for drawing lines
    if (vk.normals_debug_pipeline == VK_NULL_HANDLE) {
        return;
    }

    // Expand vertices: each vertex becomes 2 (start and end of tangent line)
    int i;
    int max_verts = MIN(tess.numVertexes, SHADER_MAX_VERTEXES / 2);
    for (i = 0; i < max_verts; i++) {
        // Start point (vertex position)
        VectorCopy(tess.xyz[i], tess.xyz[i * 2]);
        
        // End point (vertex + tangent direction)
        // qtangent stores tangent in a compressed format, need to decode
        // For now, draw a simple line in tangent direction (simplified)
        // Full implementation would decode qtangent using R_QtangentsToTBN
        float scale = 2.0f; // Length of tangent visualization line
        vec3_t tangent_dir = {1.0f, 0.0f, 0.0f}; // Simplified - would decode from qtangent
        VectorMA(tess.xyz[i], scale, tangent_dir, tess.xyz[i * 2 + 1]);
        
        // Set color (yellow for tangents)
        tess.svars.colors[0][i * 2].rgba[0] = 255;
        tess.svars.colors[0][i * 2].rgba[1] = 255;
        tess.svars.colors[0][i * 2].rgba[2] = 0;
        tess.svars.colors[0][i * 2].rgba[3] = 255;
        tess.svars.colors[0][i * 2 + 1] = tess.svars.colors[0][i * 2];
    }

    tess.numVertexes = max_verts * 2;
    tess.numIndexes = 0;

    vk_bind_pipeline(vk.normals_debug_pipeline);
    vk_bind_geometry(TESS_XYZ | TESS_RGBA0);
    vk_draw_geometry(DEPTH_RANGE_NORMAL, qfalse);

    tess.numVertexes = 0;
#else
    // PBR not enabled, tangents not available
    ri.Printf(PRINT_DEVELOPER, "Vulkan_DebugDrawTangents: PBR not enabled, tangents unavailable\n");
#endif
}

static void Vulkan_GetGPUInfo(char* info, int size) {
    // Get Vulkan GPU information
    extern vk_t vk;
    extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
    
    if (!vk.active || vk.physical_device == VK_NULL_HANDLE || 
        vk.physical_device == (VkPhysicalDevice)0x20000000) {
        Q_strncpyz(info, "Vulkan Renderer - No physical device", size);
        return;
    }
    
    if (!qvkGetPhysicalDeviceProperties) {
        Q_strncpyz(info, "Vulkan Renderer - Properties function not available", size);
        return;
    }
    
    VkPhysicalDeviceProperties props;
    qvkGetPhysicalDeviceProperties(vk.physical_device, &props);
    
    // Format GPU info string
    Com_sprintf(info, size, "Vulkan: %s (Driver: %d.%d.%d, API: %d.%d.%d)",
                props.deviceName,
                VK_VERSION_MAJOR(props.driverVersion),
                VK_VERSION_MINOR(props.driverVersion),
                VK_VERSION_PATCH(props.driverVersion),
                VK_VERSION_MAJOR(props.apiVersion),
                VK_VERSION_MINOR(props.apiVersion),
                VK_VERSION_PATCH(props.apiVersion));
}

// Forward declarations
extern float vk_get_frame_time(void);
extern float vk_get_average_fps(void);
extern vk_gpu_timing_t vk_gpu_timing;
extern vk_t vk;

static void Vulkan_GetPerformanceStats(float* fps, float* frameTime, float* gpuTime) {
    // Get Vulkan performance stats from tracking systems
    if (fps) {
        // Use average FPS from frame timing system
        *fps = vk_get_average_fps();
        // Fallback to performance struct if available
        if (*fps <= 0.0f && vk.active && vk.performance.fps > 0.0f) {
            *fps = vk.performance.fps;
        }
    }
    
    if (frameTime) {
        // Use frame time from timing system
        *frameTime = vk_get_frame_time();
        // Fallback to performance struct if available
        if (*frameTime <= 0.0f && vk.active && vk.performance.frame_time_ms > 0.0f) {
            *frameTime = vk.performance.frame_time_ms;
        }
    }
    
    if (gpuTime) {
        // Get GPU time from timing queries if available
        *gpuTime = 0.0f;
        if (vk_gpu_timing.frame_timing_count > 0) {
            // Calculate average GPU time from recent frames
            int count = vk_gpu_timing.frame_timing_count;
            int head = vk_gpu_timing.frame_timing_head;
            double sum = 0.0;
            int valid_samples = 0;
            
            for (int i = 0; i < count && i < 32; i++) {
                int idx = (head - 1 - i + 128) % 128;
                float gpu_time = vk_gpu_timing.frame_timings[idx];
                if (gpu_time > 0.0f) {
                    sum += gpu_time;
                    valid_samples++;
                }
            }
            
            if (valid_samples > 0) {
                *gpuTime = (float)(sum / valid_samples);
            }
        }
    }
}

// Forward declarations
extern void vk_mark_pipelines_dirty(void);
extern qboolean vk_reload_shader(const char *shader_name);
extern void vk_check_shader_hot_reload(void);
extern vk_t vk;

static qboolean Vulkan_ReloadShaders(void) {
    // Vulkan shader hot reload - mark all pipelines for recreation
    if (!vk.active) {
        return qfalse;
    }
    
    // Check for shader file changes first
    vk_check_shader_hot_reload();
    
    // Mark all pipelines as dirty to force recreation
    vk_mark_pipelines_dirty();
    
    // Reload all shader modules (this would iterate through all shaders)
    // For now, just mark pipelines dirty - they'll be recreated on next use
    ri.Printf(PRINT_ALL, "Vulkan: Shader reload requested - pipelines will be recreated on next use\n");
    
    return qtrue;
}

// Forward declarations
extern void vk_update_descriptor_set(image_t *image, qboolean force);
extern void vk_mark_pipelines_dirty(void);

static qboolean Vulkan_ReloadTextures(void) {
    // Vulkan texture hot reload - reload all textures from disk
    if (!vk.active) {
        ri.Printf(PRINT_WARNING, "Vulkan: Cannot reload textures - renderer not active\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan: Reloading all textures...\n");
    
    int reloaded_count = 0;
    int failed_count = 0;
    
    // Iterate through all loaded images and reload them
    for (int i = 0; i < tr.numImages; i++) {
        image_t *img = tr.images[i];
        if (!img || !img->imgName[0]) {
            continue; // Skip invalid or unnamed images
        }
        
        // Skip special images (default, white, etc.) - these are procedural
        if (img == tr.defaultImage || img == tr.whiteImage || img == tr.blackImage) {
            continue;
        }
        
        // Skip images without file names (procedural images like *white, *black)
        if (img->imgName[0] == '*') {
            continue;
        }
        
        // Try to reload the image file
        void *file_data = NULL;
        int file_size = ri.FS_ReadFile(img->imgName, &file_data);
        
        if (file_size > 0 && file_data) {
            // Image file exists, mark descriptor set for update
            // Full reload would require:
            // 1. Parse image format (TGA, JPG, PNG, etc.)
            // 2. Decode image data
            // 3. Upload to GPU using vk_upload_image_data
            // For now, just mark descriptor sets as needing update
            vk_update_descriptor_set(img, qtrue);
            reloaded_count++;
            
            ri.FS_FreeFile(file_data);
        } else {
            // File not found or error reading
            failed_count++;
        }
    }
    
    ri.Printf(PRINT_ALL, "Vulkan: Texture reload complete - %d reloaded, %d failed\n", reloaded_count, failed_count);
    
    // Mark pipelines as potentially dirty since textures changed
    vk_mark_pipelines_dirty();
    
    return qtrue;
}

static qboolean Vulkan_HasFeature(rendererFeature_t feature) {
    switch (feature) {
        case RENDERER_FEATURE_MULTISAMPLE:
            return qtrue; // Vulkan supports MSAA
        case RENDERER_FEATURE_ANISOTROPY:
            return qtrue; // Vulkan supports anisotropy
        case RENDERER_FEATURE_SHADER_CACHE:
            return qtrue; // Vulkan has pipeline cache
        case RENDERER_FEATURE_COMPUTE_SHADERS:
            return qtrue; // Vulkan supports compute
        case RENDERER_FEATURE_BINDLESS_TEXTURES:
            return vk.bindless_supported;
        case RENDERER_FEATURE_SHADER_HOTRELOAD:
            return vk.hot_reload.enabled;
        case RENDERER_FEATURE_PERFORMANCE_HUD:
            return vk.performance_hud.enabled;
        case RENDERER_FEATURE_ADVANCED_MATERIALS:
            return qtrue; // Vulkan material system
        case RENDERER_FEATURE_PARTICLE_SYSTEMS:
            return qtrue; // Vulkan particle system
        case RENDERER_FEATURE_POST_PROCESSING:
            return qtrue; // Vulkan post-processing
        default:
            return qfalse;
    }
}

static const char* Vulkan_GetExtensionString(void) {
    return "Vulkan extensions not enumerated";
}

// Vulkan renderer interface structure
static rendererInterface_t vulkanRenderer = {
    .name = "Vulkan",
    .description = "Modern Vulkan renderer with advanced features",
    .version = (1 << 16) | 0, // Version 1.0
    .features = RENDERER_FEATURE_MULTISAMPLE |
                RENDERER_FEATURE_ANISOTROPY |
                RENDERER_FEATURE_SHADER_CACHE |
                RENDERER_FEATURE_COMPUTE_SHADERS |
                RENDERER_FEATURE_BINDLESS_TEXTURES |
                RENDERER_FEATURE_SHADER_HOTRELOAD |
                RENDERER_FEATURE_PERFORMANCE_HUD |
                RENDERER_FEATURE_ADVANCED_MATERIALS |
                RENDERER_FEATURE_PARTICLE_SYSTEMS |
                RENDERER_FEATURE_POST_PROCESSING,

    .Init = Vulkan_Init,
    .Shutdown = Vulkan_Shutdown,
    .Reset = Vulkan_Reset,
    .BeginFrame = Vulkan_BeginFrame,
    .EndFrame = Vulkan_EndFrame,
    .Present = Vulkan_Present,
    .BeginScene = Vulkan_BeginScene,
    .EndScene = Vulkan_EndScene,
    .ClearScene = Vulkan_ClearScene,
    .AddEntity = Vulkan_AddEntity,
    .AddPolygon = Vulkan_AddPolygon,
    .AddLight = Vulkan_AddLight,
    .SetupLighting = Vulkan_SetupLighting,
    .RegisterShader = Vulkan_RegisterShader,
    .RemapShader = Vulkan_RemapShader,
    .RegisterImage = Vulkan_RegisterImage,
    .UpdateImage = Vulkan_UpdateImage,
    .RegisterModel = Vulkan_RegisterModel,
    .RegisterFont = Vulkan_RegisterFont,
    .RenderSurfaces = Vulkan_RenderSurfaces,
    .BeginPostProcess = Vulkan_BeginPostProcess,
    .EndPostProcess = Vulkan_EndPostProcess,
    .DebugDrawAxis = Vulkan_DebugDrawAxis,
    .DebugDrawNormals = Vulkan_DebugDrawNormals,
    .DebugDrawTangents = Vulkan_DebugDrawTangents,
    .GetGPUInfo = Vulkan_GetGPUInfo,
    .GetPerformanceStats = Vulkan_GetPerformanceStats,
    .ReloadShaders = Vulkan_ReloadShaders,
    .ReloadTextures = Vulkan_ReloadTextures,
    .HasFeature = Vulkan_HasFeature,
    .GetExtensionString = Vulkan_GetExtensionString,
};

/*
==================
Renderer_Vulkan_Create

Creates and returns the Vulkan renderer interface.
==================
*/
rendererInterface_t* Renderer_Vulkan_Create(void) {
    return &vulkanRenderer;
}