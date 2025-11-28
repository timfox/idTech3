<?php
/**
 * RenderDoc Debugging - Diagnosing the Vulkan PBR Renderer
 */
$title = 'RenderDoc Debugging - id Tech 3 Documentation';
$breadcrumbs = [
    '/renderer' => 'Renderer Deep Dive',
    '/renderer/renderdoc-debugging' => 'RenderDoc Debugging'
];
?>

<h1>RenderDoc Debugging - Diagnosing the Vulkan PBR Renderer</h1>

<div class="section">
    <h2>Overview</h2>
    <p>RenderDoc is an essential tool for debugging Vulkan applications. This walkthrough covers how to integrate RenderDoc with JKSunny's PBR port, capture frames, and diagnose common rendering issues in the Vulkan+PBR pipeline.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li><strong>RenderDoc Integration:</strong> Setting up captures in the engine</li>
            <li><strong>Frame Analysis:</strong> Understanding draw calls and pipeline states</li>
            <li><strong>Resource Inspection:</strong> Debugging textures, buffers, and descriptors</li>
            <li><strong>Performance Profiling:</strong> Identifying bottlenecks and optimization opportunities</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>RenderDoc Setup and Integration</h2>
    
    <h3>Engine Integration</h3>
    <div class="code-block">
        <pre><code>// tr_renderdoc.c - RenderDoc integration for debugging
#ifdef USE_RENDERDOC
#include "renderdoc_app.h"

typedef struct renderDocAPI_s {
    RENDERDOC_API_1_4_0* rdoc_api;
    qboolean initialized;
    qboolean captureInProgress;
    int captureFrame;
    
    // Integration controls
    cvar_t* rd_enable;
    cvar_t* rd_captureFrame;
    cvar_t* rd_captureNextFrame;
    cvar_t* rd_captureOnError;
    
} renderDocAPI_t;

static renderDocAPI_t rd;

qboolean RenderDoc_Init(void) {
    Com_Printf("Initializing RenderDoc integration\n");
    
    // Register CVars
    rd.rd_enable = Cvar_Get("rd_enable", "1", CVAR_ARCHIVE);
    rd.rd_captureFrame = Cvar_Get("rd_captureFrame", "-1", 0);
    rd.rd_captureNextFrame = Cvar_Get("rd_captureNextFrame", "0", 0);
    rd.rd_captureOnError = Cvar_Get("rd_captureOnError", "1", CVAR_ARCHIVE);
    
    if (!rd.rd_enable->integer) {
        Com_Printf("RenderDoc integration disabled\n");
        return qfalse;
    }
    
    // Try to get RenderDoc API
    if (RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void**)&rd.rdoc_api) != 1) {
        Com_Printf("RenderDoc not available\n");
        return qfalse;
    }
    
    // Configure RenderDoc settings
    rd.rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_AllowVSync, 0);
    rd.rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_VerifyMapWrites, 1);
    rd.rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_SaveAllInitials, 1);
    rd.rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks, 1);
    rd.rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureAllCmdLists, 1);
    
    // Set up capture annotations
    rd.rdoc_api->SetCaptureFilePathTemplate("captures/quake3e_frame");
    
    rd.initialized = qtrue;
    Com_Printf("RenderDoc integration initialized\n");
    
    // Register console commands
    Cmd_AddCommand("rd_capture", RenderDoc_CaptureFrame_f);
    Cmd_AddCommand("rd_trigger", RenderDoc_TriggerCapture_f);
    Cmd_AddCommand("rd_launch", RenderDoc_LaunchReplay_f);
    
    return qtrue;
}

void RenderDoc_CaptureFrame_f(void) {
    if (!rd.initialized) {
        Com_Printf("RenderDoc not initialized\n");
        return;
    }
    
    rd.rd_captureNextFrame->integer = 1;
    Com_Printf("RenderDoc: Will capture next frame\n");
}

void RenderDoc_TriggerCapture_f(void) {
    if (!rd.initialized) {
        Com_Printf("RenderDoc not initialized\n");
        return;
    }
    
    rd.rdoc_api->TriggerCapture();
    Com_Printf("RenderDoc: Triggered immediate capture\n");
}

void RenderDoc_LaunchReplay_f(void) {
    if (!rd.initialized) {
        Com_Printf("RenderDoc not initialized\n");
        return;
    }
    
    if (rd.rdoc_api->IsTargetControlConnected()) {
        rd.rdoc_api->LaunchReplayUI(1, NULL);
        Com_Printf("RenderDoc: Launched replay UI\n");
    } else {
        Com_Printf("RenderDoc: Target control not connected\n");
    }
}

void RenderDoc_BeginFrame(void) {
    if (!rd.initialized || !rd.rd_enable->integer) {
        return;
    }
    
    static int frameCount = 0;
    frameCount++;
    
    // Check for specific frame capture
    if (rd.rd_captureFrame->integer == frameCount) {
        rd.rdoc_api->StartFrameCapture(NULL, NULL);
        rd.captureInProgress = qtrue;
        rd.captureFrame = frameCount;
        Com_Printf("RenderDoc: Started capture for frame %d\n", frameCount);
    }
    
    // Check for next frame capture
    if (rd.rd_captureNextFrame->integer) {
        rd.rdoc_api->StartFrameCapture(NULL, NULL);
        rd.captureInProgress = qtrue;
        rd.captureFrame = frameCount;
        rd.rd_captureNextFrame->integer = 0;
        Com_Printf("RenderDoc: Started capture for frame %d\n", frameCount);
    }
}

void RenderDoc_EndFrame(void) {
    if (!rd.initialized || !rd.captureInProgress) {
        return;
    }
    
    rd.rdoc_api->EndFrameCapture(NULL, NULL);
    rd.captureInProgress = qfalse;
    
    Com_Printf("RenderDoc: Ended capture for frame %d\n", rd.captureFrame);
    Com_Printf("RenderDoc: Capture saved to captures/quake3e_frame_%d.rdc\n", rd.captureFrame);
}

void RenderDoc_BeginEvent(const char* name) {
    if (!rd.initialized || !rd.rd_enable->integer) {
        return;
    }
    
    rd.rdoc_api->StartFrameCapture(NULL, NULL);
}

void RenderDoc_EndEvent(void) {
    if (!rd.initialized || !rd.rd_enable->integer) {
        return;
    }
    
    rd.rdoc_api->EndFrameCapture(NULL, NULL);
}

// Convenient macros for event annotation
#define RD_SCOPED_EVENT(name) \
    RenderDoc_BeginEvent(name); \
    defer(RenderDoc_EndEvent())

#define RD_BEGIN_EVENT(name) RenderDoc_BeginEvent(name)
#define RD_END_EVENT() RenderDoc_EndEvent()

#else // !USE_RENDERDOC

// Stub implementations when RenderDoc is disabled
#define RD_SCOPED_EVENT(name)
#define RD_BEGIN_EVENT(name)
#define RD_END_EVENT()

qboolean RenderDoc_Init(void) { return qfalse; }
void RenderDoc_BeginFrame(void) {}
void RenderDoc_EndFrame(void) {}

#endif // USE_RENDERDOC</code></pre>
    </div>
    
    <h3>Build Configuration</h3>
    <div class="code-block">
        <pre><code># CMakeLists.txt - Adding RenderDoc support
option(USE_RENDERDOC "Enable RenderDoc integration" ON)

if(USE_RENDERDOC)
    find_package(PkgConfig QUIET)
    
    # Try to find RenderDoc
    if(WIN32)
        find_path(RENDERDOC_INCLUDE_DIR
            NAMES renderdoc_app.h
            PATHS
                "$ENV{PROGRAMFILES}/RenderDoc"
                "$ENV{PROGRAMFILES(X86)}/RenderDoc"
            PATH_SUFFIXES include
        )
    else()
        pkg_check_modules(RENDERDOC QUIET renderdoc)
        if(RENDERDOC_FOUND)
            set(RENDERDOC_INCLUDE_DIR ${RENDERDOC_INCLUDE_DIRS})
        endif()
    endif()
    
    if(RENDERDOC_INCLUDE_DIR)
        message(STATUS "Found RenderDoc: ${RENDERDOC_INCLUDE_DIR}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_RENDERDOC)
        target_include_directories(${PROJECT_NAME} PRIVATE ${RENDERDOC_INCLUDE_DIR})
    else()
        message(WARNING "RenderDoc not found, debugging features disabled")
        set(USE_RENDERDOC OFF)
    endif()
endif()

# Add RenderDoc source files
if(USE_RENDERDOC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/renderer/tr_renderdoc.c
    )
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Annotating the PBR Pipeline</h2>
    
    <h3>Adding Debug Markers</h3>
    <div class="code-block">
        <pre><code>// Adding RenderDoc annotations to the PBR rendering pipeline
void PBR_RenderFrame(void) {
    RD_SCOPED_EVENT("PBR_RenderFrame");
    
    // Begin frame capture if requested
    RenderDoc_BeginFrame();
    
    VkCommandBuffer cmd = vk.commandBuffers[vk.currentFrame];
    
    // Begin render pass
    RD_BEGIN_EVENT("BeginRenderPass");
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = vk.renderPass,
        .framebuffer = vk.framebuffers[vk.currentImageIndex],
        .renderArea.offset = {0, 0},
        .renderArea.extent = vk.swapchainExtent,
        .clearValueCount = 2,
        .pClearValues = clearValues,
    };
    
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    RD_END_EVENT();
    
    // Update uniform buffers
    RD_BEGIN_EVENT("UpdateUniforms");
    PBR_UpdateCameraUniforms(&tr.refdef);
    PBR_UpdateLightingUniforms();
    RD_END_EVENT();
    
    // Bind PBR pipeline
    RD_BEGIN_EVENT("BindPipeline");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);
    RD_END_EVENT();
    
    // Render world geometry
    RD_BEGIN_EVENT("RenderWorld");
    PBR_RenderWorld();
    RD_END_EVENT();
    
    // Render entities
    RD_BEGIN_EVENT("RenderEntities");
    PBR_RenderEntities();
    RD_END_EVENT();
    
    // Render transparent objects
    RD_BEGIN_EVENT("RenderTransparent");
    PBR_RenderTransparent();
    RD_END_EVENT();
    
    // Post-processing
    RD_BEGIN_EVENT("PostProcessing");
    // TODO: Add post-processing passes
    RD_END_EVENT();
    
    // End render pass
    RD_BEGIN_EVENT("EndRenderPass");
    vkCmdEndRenderPass(cmd);
    RD_END_EVENT();
    
    // End frame capture if in progress
    RenderDoc_EndFrame();
}

void PBR_DrawSurface(msurface_t* surface, pbrMaterial_t* material) {
    char eventName[256];
    Com_sprintf(eventName, sizeof(eventName), "DrawSurface_%s", 
               material ? material->name : "default");
    
    RD_SCOPED_EVENT(eventName);
    
    VkCommandBuffer cmd = vk.commandBuffers[vk.currentFrame];
    
    // Bind material descriptor set
    RD_BEGIN_EVENT("BindMaterial");
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pbrPipelineLayout, 0, 1, &material->descriptorSet,
                           0, NULL);
    RD_END_EVENT();
    
    // Bind vertex/index buffers
    RD_BEGIN_EVENT("BindGeometry");
    VkBuffer vertexBuffers[] = {surface->vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, surface->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    RD_END_EVENT();
    
    // Draw
    RD_BEGIN_EVENT("DrawIndexed");
    vkCmdDrawIndexed(cmd, surface->numIndexes, 1, 0, 0, 0);
    RD_END_EVENT();
}

// Memory allocation debugging
void* VMA_AllocDebug(VmaAllocator allocator, const VmaAllocationCreateInfo* createInfo,
                     VmaAllocation* allocation, VmaAllocationInfo* allocationInfo,
                     const char* file, int line) {
    
    char eventName[256];
    Com_sprintf(eventName, sizeof(eventName), "VMA_Alloc_%s:%d", file, line);
    
    RD_BEGIN_EVENT(eventName);
    VkResult result = vmaAllocateMemory(allocator, createInfo, allocation, allocationInfo);
    RD_END_EVENT();
    
    if (result != VK_SUCCESS) {
        Com_Printf("^1VMA allocation failed at %s:%d: %s\n", 
                  file, line, VK_ResultToString(result));
    }
    
    return result == VK_SUCCESS ? allocationInfo->pMappedData : NULL;
}

// Wrap VMA calls with debug info
#ifdef USE_RENDERDOC
#define VMA_ALLOC(allocator, createInfo, allocation, allocInfo) \
    VMA_AllocDebug(allocator, createInfo, allocation, allocInfo, __FILE__, __LINE__)
#else
#define VMA_ALLOC(allocator, createInfo, allocation, allocInfo) \
    vmaAllocateMemory(allocator, createInfo, allocation, allocInfo)
#endif</code></pre>
    </div>
    
    <h3>Conditional Debugging</h3>
    <div class="code-block">
        <pre><code>// Conditional debugging based on CVars and error conditions
void PBR_DebugCapture(const char* reason) {
    if (!rd.initialized || !rd.rd_captureOnError->integer) {
        return;
    }
    
    Com_Printf("^3RenderDoc: Auto-capturing frame due to %s\n", reason);
    
    // Set up for next frame capture
    rd.rd_captureNextFrame->integer = 1;
}

// Automatic capture on Vulkan validation errors
VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData) {
    
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        Com_Printf("^1Vulkan ERROR: [%s] %s\n", pLayerPrefix, pMessage);
        
        // Trigger RenderDoc capture on errors
        PBR_DebugCapture("Vulkan validation error");
        
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        Com_Printf("^3Vulkan WARNING: [%s] %s\n", pLayerPrefix, pMessage);
    }
    
    return VK_FALSE;
}

// Performance-based captures
void PBR_CheckPerformanceCapture(void) {
    static float frameTimeHistory[60];
    static int frameIndex = 0;
    static int frameCount = 0;
    
    frameTimeHistory[frameIndex] = tr.frameTime;
    frameIndex = (frameIndex + 1) % 60;
    frameCount++;
    
    if (frameCount < 60) {
        return; // Need full history
    }
    
    // Calculate average frame time
    float totalTime = 0;
    for (int i = 0; i < 60; i++) {
        totalTime += frameTimeHistory[i];
    }
    float avgFrameTime = totalTime / 60.0f;
    float targetFrameTime = 1000.0f / 60.0f; // 60 FPS target
    
    // Capture if performance is poor
    if (avgFrameTime > targetFrameTime * 1.5f) {
        static int lastPerfCapture = 0;
        int currentTime = Sys_Milliseconds();
        
        if (currentTime - lastPerfCapture > 10000) { // Don't spam captures
            PBR_DebugCapture("poor performance");
            lastPerfCapture = currentTime;
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Analyzing Captured Frames</h2>
    
    <h3>Understanding the Event Browser</h3>
    <div class="code-block">
        <pre><code>// Understanding RenderDoc capture structure for PBR pipeline:

// Typical frame structure you'll see in RenderDoc:
/*
Frame #1234
└── PBR_RenderFrame
    ├── BeginRenderPass
    ├── UpdateUniforms
    │   ├── Camera UBO Update
    │   └── Lighting UBO Update
    ├── BindPipeline
    ├── RenderWorld
    │   ├── DrawSurface_concrete_floor
    │   │   ├── BindMaterial (4 textures + material UBO)
    │   │   ├── BindGeometry (vertex + index buffers)
    │   │   └── DrawIndexed (1,234 triangles)
    │   ├── DrawSurface_brick_wall
    │   └── DrawSurface_metal_trim
    ├── RenderEntities
    │   ├── DrawSurface_player_body
    │   ├── DrawSurface_weapon_shotgun
    │   └── DrawSurface_pickup_armor
    ├── RenderTransparent
    │   ├── DrawSurface_glass_window
    │   └── DrawSurface_water_surface
    ├── PostProcessing
    └── EndRenderPass
*/</code></pre>
    </div>
    
    <h3>Key Areas to Inspect</h3>
    <div class="code-block">
        <pre><code>// RenderDoc Analysis Checklist for PBR Pipeline

// 1. PIPELINE STATE VERIFICATION
/*
In RenderDoc Pipeline State tab, verify:
- Vertex shader: pbr.vert.spv loaded correctly
- Fragment shader: pbr.frag.spv loaded correctly
- Vertex input layout matches drawVert_t:
  * Location 0: Position (R32G32B32_SFLOAT)
  * Location 1: Normal (R32G32B32_SFLOAT)
  * Location 2: TexCoord (R32G32_SFLOAT)
  * Location 3: Tangent (R32G32B32A32_SFLOAT)
  * Location 4: Color (R8G8B8A8_UNORM)
- Rasterization state: correct cull mode, polygon mode
- Depth/stencil state: depth test enabled, correct compare op
- Blend state: appropriate for opaque/transparent objects
*/

// 2. DESCRIPTOR SET VALIDATION
/*
Check each material's descriptor set bindings:
Set 0 (Material):
- Binding 0: Base Color texture (COMBINED_IMAGE_SAMPLER)
- Binding 1: Normal texture (COMBINED_IMAGE_SAMPLER)
- Binding 2: Metallic-Roughness texture (COMBINED_IMAGE_SAMPLER)
- Binding 3: Occlusion texture (COMBINED_IMAGE_SAMPLER)
- Binding 4: Emissive texture (COMBINED_IMAGE_SAMPLER)
- Binding 5: Material properties UBO (UNIFORM_BUFFER)

Set 1 (Uniforms):
- Binding 0: Camera UBO (view/projection matrices)
- Binding 1: Lighting UBO (lights, IBL settings)

Set 2 (IBL):
- Binding 0: Environment cubemap
- Binding 1: Irradiance cubemap
- Binding 2: Prefiltered environment map
- Binding 3: BRDF LUT
*/

// 3. COMMON ISSUES TO LOOK FOR
/*
Black materials:
- Check base color texture is bound and valid
- Verify material UBO has correct baseColor values
- Ensure normal map is in correct format (RG or RGB)

Incorrect lighting:
- Verify directional light direction is normalized
- Check IBL cubemaps are properly bound
- Ensure view position is correct in camera UBO

Performance issues:
- Look for excessive draw calls (should batch by material)
- Check for large index/vertex buffers
- Verify texture sizes are appropriate
- Look for redundant state changes

Memory issues:
- Check for large texture uploads
- Verify staging buffers are being reused
- Look for descriptor set creation patterns
*/</code></pre>
    </div>
</div>

<div class="section">
    <h2>Common Debugging Scenarios</h2>
    
    <h3>Scenario 1: Black Materials</h3>
    <div class="code-block">
        <pre><code>// Debugging black/incorrect materials in RenderDoc

/*
Step 1: Select a draw call with black material
- In Event Browser, find DrawSurface_[material_name]
- Click on the DrawIndexed event

Step 2: Check Pipeline State
- Go to Pipeline State tab
- Verify fragment shader is bound (pbr.frag.spv)
- Check if vertex attributes are properly mapped

Step 3: Inspect Descriptor Sets
- In Pipeline State, expand "Descriptor Sets"
- Click on Set 0 (Material descriptors)
- For each texture binding:
  * Click the thumbnail to view texture
  * Check if texture data looks correct
  * Verify format (should be R8G8B8A8_UNORM for albedo)
  * Check sampler settings

Step 4: Check Material UBO
- Still in Set 0, click on the UBO binding (usually binding 5)
- In Resource Inspector, verify material parameters:
  * baseColor should be (1,1,1) or material color
  * metallicFactor should be 0.0-1.0
  * roughnessFactor should be 0.0-1.0
  * normalScale should be ~1.0

Step 5: Verify Shader Inputs
- In Mesh Viewer tab, check vertex data:
  * Positions should be reasonable world coordinates
  * Normals should be normalized (-1 to +1)
  * UV coordinates should be 0-1 range
  * Tangents should be normalized with W component

Step 6: Debug Fragment Shader
- In Pixel History tab, select a pixel that should be lit
- Step through fragment shader execution
- Check intermediate values:
  * Normal mapping calculation
  * Material parameter sampling
  * Lighting calculations
*/

// Console commands to help debug:
Cmd_AddCommand("debug_material", Debug_DumpMaterial_f);
Cmd_AddCommand("debug_lighting", Debug_DumpLighting_f);

void Debug_DumpMaterial_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: debug_material <surface_id>\n");
        return;
    }
    
    int surfaceId = atoi(Cmd_Argv(1));
    // Find surface and dump material properties
    
    Com_Printf("Material Debug Info:\n");
    Com_Printf("  Base Color: %.2f %.2f %.2f\n", 
              material->baseColor[0], material->baseColor[1], material->baseColor[2]);
    Com_Printf("  Metallic: %.2f\n", material->metallicFactor);
    Com_Printf("  Roughness: %.2f\n", material->roughnessFactor);
    // ... dump all material properties
}</code></pre>
    </div>
    
    <h3>Scenario 2: Performance Issues</h3>
    <div class="code-block">
        <pre><code>// Using RenderDoc to identify performance bottlenecks

/*
Step 1: Capture a slow frame
- Use rd_capture console command during heavy scene
- Or set rd_captureOnError 1 to auto-capture on perf issues

Step 2: Analyze Draw Call Count
- In Event Browser, expand PBR_RenderFrame
- Count DrawIndexed events
- Look for patterns:
  * Too many small draw calls? (should batch)
  * Large vertex/index counts? (check LOD system)
  * Redundant state changes? (should sort by material)

Step 3: Check GPU Timeline
- Go to Timeline tab in RenderDoc
- Look for gaps or long operations:
  * Large texture uploads (should use staging efficiently)
  * Synchronization stalls (fence waits)
  * Long draw calls (check triangle count)

Step 4: Analyze Memory Usage
- In Statistics tab, check:
  * Total vertex/index data size
  * Texture memory usage
  * Uniform buffer updates per frame
  * Descriptor set creation count

Step 5: Profile Individual Draw Calls
- Select heaviest DrawIndexed event
- In Statistics tab for that event:
  * Triangle count (aim for 500-5000 per call)
  * Vertex count vs unique vertices (check indexing)
  * Overdraw analysis (pixel shader invocations vs pixels)

Step 6: Check Resource Reuse
- Look for repeated texture/buffer creation
- Verify staging buffers are reused
- Check descriptor set allocation patterns
*/

// Performance monitoring integration
void PBR_UpdatePerformanceCounters(void) {
    static performanceCounters_t perfCounters;
    
    perfCounters.frameTime = tr.frameTime;
    perfCounters.drawCalls = tr.pc.c_drawCalls;
    perfCounters.triangles = tr.pc.c_triangles;
    perfCounters.textureUploads = tr.pc.c_textureUploads;
    
    // Trigger capture if performance drops
    if (perfCounters.frameTime > 33.33f) { // Below 30 FPS
        static int lastPerfCapture = 0;
        if (Sys_Milliseconds() - lastPerfCapture > 5000) {
            PBR_DebugCapture("performance drop");
            lastPerfCapture = Sys_Milliseconds();
        }
    }
}</code></pre>
    </div>
    
    <h3>Scenario 3: Texture Issues</h3>
    <div class="code-block">
        <pre><code>// Debugging texture loading and sampling issues

/*
Step 1: Identify Problematic Texture
- Find draw call with incorrect textures
- In Pipeline State, check material descriptor set
- Click on texture binding thumbnails

Step 2: Verify Texture Content
- In Texture Viewer:
  * Check if image data looks correct
  * Verify dimensions (power of 2 for mipmaps)
  * Check format (R8G8B8A8_UNORM for albedo, etc.)
  * Examine mip levels (should gradually reduce detail)

Step 3: Check Sampler Settings
- In Pipeline State, find sampler configuration:
  * Filter mode (LINEAR for smooth sampling)
  * Address mode (REPEAT for tiling textures)
  * Anisotropy settings (up to 16x for quality)
  * Mip LOD settings (minLod: 0, maxLod: all levels)

Step 4: Verify Upload Process
- Look for vkCmdCopyBufferToImage events
- Check staging buffer content before copy
- Verify image layout transitions:
  * UNDEFINED -> TRANSFER_DST_OPTIMAL (for upload)
  * TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL (for use)

Step 5: Debug UV Coordinates
- In Mesh Viewer, check texture coordinates:
  * Should be 0-1 range for clamped textures
  * Can be outside 0-1 for repeated textures
  * Look for degenerate triangles (NaN/Inf coords)

Step 6: Test with Known Good Texture
- Replace problematic texture with default white texture
- If rendering improves, issue is in texture content/format
- If still wrong, issue is in shader or UV mapping
*/

// Texture debugging utilities
void Debug_SaveTexture_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: debug_savetexture <name>\n");
        return;
    }
    
    const char* texName = Cmd_Argv(1);
    texture_t* tex = R_FindTexture(texName);
    
    if (!tex) {
        Com_Printf("Texture '%s' not found\n", texName);
        return;
    }
    
    // Download texture from GPU and save to disk
    VK_DownloadTexture(tex, va("debug_%s.png", texName));
    Com_Printf("Saved texture to debug_%s.png\n", texName);
}

void Debug_ListTextures_f(void) {
    Com_Printf("Loaded textures:\n");
    for (int i = 0; i < numTextures; i++) {
        texture_t* tex = &textures[i];
        Com_Printf("  %3d: %s (%dx%d, %d mips) - last used frame %d\n",
                  i, tex->name, tex->width, tex->height, 
                  tex->mipLevels, tex->frameUsed);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced RenderDoc Techniques</h2>
    
    <h3>Custom Annotations and Grouping</h3>
    <div class="code-block">
        <pre><code>// Advanced RenderDoc annotation techniques for complex scenes

// Hierarchical event grouping
void PBR_RenderWorldSector(worldSector_t* sector) {
    char eventName[128];
    Com_sprintf(eventName, sizeof(eventName), 
               "WorldSector_%d (%d surfaces)", 
               sector->id, sector->numSurfaces);
    
    RD_SCOPED_EVENT(eventName);
    
    // Group by material type for easier analysis
    RD_BEGIN_EVENT("Opaque_Surfaces");
    for (int i = 0; i < sector->numSurfaces; i++) {
        if (!(sector->surfaces[i]->shader->contentFlags & CONTENTS_TRANSLUCENT)) {
            PBR_DrawSurface(sector->surfaces[i], sector->materials[i]);
        }
    }
    RD_END_EVENT();
    
    RD_BEGIN_EVENT("Transparent_Surfaces");
    for (int i = 0; i < sector->numSurfaces; i++) {
        if (sector->surfaces[i]->shader->contentFlags & CONTENTS_TRANSLUCENT) {
            PBR_DrawSurface(sector->surfaces[i], sector->materials[i]);
        }
    }
    RD_END_EVENT();
}

// Material-based grouping for batch analysis
void PBR_RenderByMaterial(void) {
    RD_SCOPED_EVENT("PBR_RenderByMaterial");
    
    // Sort surfaces by material to minimize state changes
    qsort(renderSurfaces, numRenderSurfaces, sizeof(renderSurface_t), 
          CompareSurfacesByMaterial);
    
    pbrMaterial_t* currentMaterial = NULL;
    int batchStart = 0;
    
    for (int i = 0; i <= numRenderSurfaces; i++) {
        pbrMaterial_t* material = (i < numRenderSurfaces) ? 
                                 renderSurfaces[i].material : NULL;
        
        if (material != currentMaterial || i == numRenderSurfaces) {
            if (currentMaterial && i > batchStart) {
                char batchName[128];
                Com_sprintf(batchName, sizeof(batchName), 
                           "Material_%s (%d surfaces)", 
                           currentMaterial->name, i - batchStart);
                
                RD_BEGIN_EVENT(batchName);
                
                // Bind material once for entire batch
                PBR_BindMaterial(currentMaterial);
                
                // Draw all surfaces with this material
                for (int j = batchStart; j < i; j++) {
                    PBR_DrawSurfaceGeometry(&renderSurfaces[j]);
                }
                
                RD_END_EVENT();
            }
            
            currentMaterial = material;
            batchStart = i;
        }
    }
}

// LOD visualization and debugging
void PBR_RenderWithLODAnnotations(void) {
    RD_SCOPED_EVENT("PBR_LOD_Rendering");
    
    const char* lodNames[] = {"LOD0_Highest", "LOD1_High", "LOD2_Medium", "LOD3_Low"};
    
    for (int lod = 0; lod < 4; lod++) {
        RD_BEGIN_EVENT(lodNames[lod]);
        
        for (int i = 0; i < numRenderEntities; i++) {
            if (renderEntities[i].lodLevel == lod) {
                char entityName[64];
                Com_sprintf(entityName, sizeof(entityName), 
                           "Entity_%d_LOD%d", renderEntities[i].entityNum, lod);
                
                RD_BEGIN_EVENT(entityName);
                PBR_DrawEntity(&renderEntities[i]);
                RD_END_EVENT();
            }
        }
        
        RD_END_EVENT();
    }
}</code></pre>
    </div>
    
    <h3>Memory and Resource Tracking</h3>
    <div class="code-block">
        <pre><code>// Advanced resource tracking for memory debugging

typedef struct renderDocResourceTracker_s {
    struct {
        int allocated;
        int freed;
        size_t totalSize;
        size_t peakSize;
    } buffers;
    
    struct {
        int allocated;
        int freed;
        size_t totalSize;
        size_t peakSize;
    } images;
    
    struct {
        int created;
        int destroyed;
        int peak;
    } descriptorSets;
    
} renderDocResourceTracker_t;

static renderDocResourceTracker_t rdResourceTracker;

void RD_TrackBufferAllocation(size_t size) {
    rdResourceTracker.buffers.allocated++;
    rdResourceTracker.buffers.totalSize += size;
    
    if (rdResourceTracker.buffers.totalSize > rdResourceTracker.buffers.peakSize) {
        rdResourceTracker.buffers.peakSize = rdResourceTracker.buffers.totalSize;
    }
    
    // Add annotation for large allocations
    if (size > 1024 * 1024) { // 1MB+
        char annotation[128];
        Com_sprintf(annotation, sizeof(annotation), 
                   "Large_Buffer_Alloc_%zuMB", size / (1024 * 1024));
        RD_BEGIN_EVENT(annotation);
        RD_END_EVENT();
    }
}

void RD_TrackBufferFree(size_t size) {
    rdResourceTracker.buffers.freed++;
    rdResourceTracker.buffers.totalSize -= size;
}

// Resource leak detection
void RD_CheckResourceLeaks(void) {
    char leakInfo[512];
    
    int bufferLeaks = rdResourceTracker.buffers.allocated - rdResourceTracker.buffers.freed;
    int imageLeaks = rdResourceTracker.images.allocated - rdResourceTracker.images.freed;
    int descriptorLeaks = rdResourceTracker.descriptorSets.created - 
                         rdResourceTracker.descriptorSets.destroyed;
    
    if (bufferLeaks > 0 || imageLeaks > 0 || descriptorLeaks > 0) {
        Com_sprintf(leakInfo, sizeof(leakInfo),
                   "Resource_Leaks: Buffers=%d Images=%d Descriptors=%d",
                   bufferLeaks, imageLeaks, descriptorLeaks);
        
        RD_BEGIN_EVENT(leakInfo);
        RD_END_EVENT();
        
        Com_Printf("^1Resource leaks detected: %s\n", leakInfo);
    }
}

// Automatic capture triggers
void RD_SetupAutomaticCaptures(void) {
    // Capture on validation errors
    if (rd.rd_captureOnError->integer) {
        // Already handled in validation callback
    }
    
    // Capture on memory pressure
    Cvar_RegisterCallback(Memory_CheckPressure_Callback, "r_memoryPressure");
    
    // Capture on performance drops
    static int consecutiveSlowFrames = 0;
    if (tr.frameTime > 33.33f) { // > 33ms (30 FPS)
        consecutiveSlowFrames++;
        if (consecutiveSlowFrames > 5) {
            PBR_DebugCapture("consecutive slow frames");
            consecutiveSlowFrames = 0;
        }
    } else {
        consecutiveSlowFrames = 0;
    }
}

void Memory_CheckPressure_Callback(cvar_t* cvar) {
    if (cvar->integer > 0) {
        PBR_DebugCapture("memory pressure detected");
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>RenderDoc Best Practices</h2>
    
    <h3>Efficient Workflow</h3>
    <div class="code-block">
        <pre><code>// Best practices for using RenderDoc with large game engines

/*
1. STRATEGIC CAPTURE TIMING
- Don't capture every frame (performance impact)
- Use specific triggers: errors, performance drops, specific scenes
- Capture representative frames, not random ones
- Use frame ranges for animation debugging

2. MEANINGFUL ANNOTATIONS
- Use hierarchical event names (System_Subsystem_Operation)
- Include quantitative data in names (MaterialBatch_Metal_15_Surfaces)
- Group related operations together
- Use consistent naming conventions

3. EFFICIENT ANALYSIS
- Start with high-level overview (timeline, statistics)
- Use filtering to focus on specific issues
- Compare good vs bad frames side-by-side
- Save frequently used analysis as custom views

4. PERFORMANCE CONSIDERATIONS
- Disable RenderDoc in release builds
- Use conditional compilation for annotations
- Minimize string formatting in hot paths
- Cache annotation strings when possible

5. TEAM COLLABORATION
- Include capture files in bug reports
- Document analysis findings
- Share common debugging techniques
- Maintain library of reference captures
*/

// Optimized annotation system
typedef struct cachedAnnotation_s {
    char name[64];
    qboolean inUse;
} cachedAnnotation_t;

#define MAX_CACHED_ANNOTATIONS 256
static cachedAnnotation_t annotationCache[MAX_CACHED_ANNOTATIONS];
static int numCachedAnnotations = 0;

const char* RD_GetCachedAnnotation(const char* format, ...) {
    static char buffer[64];
    va_list args;
    
    va_start(args, format);
    Q_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Check cache first
    for (int i = 0; i < numCachedAnnotations; i++) {
        if (!strcmp(annotationCache[i].name, buffer)) {
            return annotationCache[i].name;
        }
    }
    
    // Add to cache if space available
    if (numCachedAnnotations < MAX_CACHED_ANNOTATIONS) {
        Q_strncpyz(annotationCache[numCachedAnnotations].name, buffer, 
                  sizeof(annotationCache[0].name));
        return annotationCache[numCachedAnnotations++].name;
    }
    
    return buffer; // Fallback to temporary buffer
}

// Use cached annotations for frequently called functions
void PBR_DrawSurfaceOptimized(msurface_t* surface, pbrMaterial_t* material) {
    const char* annotation = RD_GetCachedAnnotation("Draw_%s", material->name);
    
    RD_SCOPED_EVENT(annotation);
    
    // ... rest of draw function
}</code></pre>
    </div>
    
    <h3>Debugging Checklist</h3>
    <div class="code-block">
        <pre><code>// Comprehensive debugging checklist for RenderDoc analysis

/*
INITIAL SETUP CHECKLIST:
□ RenderDoc integrated and detecting application
□ Validation layers enabled in debug builds
□ Automatic capture triggers configured
□ Event annotations added to key functions
□ Resource tracking enabled

FRAME CAPTURE CHECKLIST:
□ Captured frame represents the issue
□ All relevant events are annotated
□ Resource states are valid
□ No validation errors in captured frame
□ Performance counters available

ANALYSIS WORKFLOW:
□ Check Event Browser for overall structure
□ Verify Pipeline State for each problematic draw
□ Inspect all descriptor set bindings
□ Validate vertex/index buffer content
□ Check texture content and formats
□ Review uniform buffer values
□ Analyze GPU timeline for bottlenecks
□ Compare with known-good reference frame

COMMON ISSUES TO CHECK:
□ Black materials (texture binding, UV coords)
□ Incorrect lighting (uniform values, IBL setup)
□ Performance problems (draw call count, batching)
□ Memory leaks (resource allocation patterns)
□ Validation errors (resource usage, synchronization)
□ Rendering artifacts (depth testing, blending)

DOCUMENTATION:
□ Save capture files with descriptive names
□ Document findings and solutions
□ Add comments to problematic areas
□ Share analysis with team members
□ Update debugging procedures based on findings
*/

// Automated issue detection
void RD_AutoAnalyzeFrame(void) {
    if (!rd.initialized) {
        return;
    }
    
    // Check for common issues and annotate them
    static analysisResults_t results;
    memset(&results, 0, sizeof(results));
    
    // Analyze draw call patterns
    if (tr.pc.c_drawCalls > 1000) {
        results.flags |= ANALYSIS_TOO_MANY_DRAWS;
        RD_BEGIN_EVENT("WARNING_High_Draw_Call_Count");
        RD_END_EVENT();
    }
    
    // Check for texture thrashing
    if (tr.pc.c_textureUploads > 50) {
        results.flags |= ANALYSIS_TEXTURE_THRASHING;
        RD_BEGIN_EVENT("WARNING_Excessive_Texture_Uploads");
        RD_END_EVENT();
    }
    
    // Memory pressure detection
    if (VMA_GetCurrentUsage() > VMA_GetBudget() * 0.9f) {
        results.flags |= ANALYSIS_MEMORY_PRESSURE;
        RD_BEGIN_EVENT("WARNING_High_Memory_Usage");
        RD_END_EVENT();
    }
    
    // Log analysis results
    if (results.flags) {
        Com_Printf("RenderDoc: Frame analysis flags: 0x%08x\n", results.flags);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/renderer/vulkan-implementation">Vulkan Renderer</a></li>
        <li><a href="/renderer/pbr-pipeline">PBR Pipeline</a></li>
        <li><a href="/renderer/resource-management">Resource Management</a></li>
        <li><a href="/modernization/profiling-tools">Profiling Tools</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
    </ul>
</div>