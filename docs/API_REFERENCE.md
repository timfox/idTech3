# Engine API Reference

## Unified Renderer Interface

The engine provides a unified renderer interface that abstracts graphics API differences, allowing seamless fallback between Vulkan, OpenGL2, and Metal renderers.

### Renderer Features

```c
typedef enum {
    RENDERER_FEATURE_MULTISAMPLE        = (1 << 0),  // MSAA support
    RENDERER_FEATURE_ANISOTROPY         = (1 << 1),  // Anisotropic filtering
    RENDERER_FEATURE_SHADER_CACHE       = (1 << 2),  // Pipeline caching
    RENDERER_FEATURE_COMPUTE_SHADERS    = (1 << 3),  // Compute shader support
    RENDERER_FEATURE_RAYTRACING         = (1 << 4),  // Ray tracing support
    RENDERER_FEATURE_BINDLESS_TEXTURES  = (1 << 5),  // Bindless textures
    RENDERER_FEATURE_SHADER_HOTRELOAD   = (1 << 6),  // Shader hot reloading
    RENDERER_FEATURE_PERFORMANCE_HUD    = (1 << 7),  // Performance HUD
    RENDERER_FEATURE_ADVANCED_MATERIALS = (1 << 8),  // Advanced materials
    RENDERER_FEATURE_PARTICLE_SYSTEMS   = (1 << 9),  // Particle systems
    RENDERER_FEATURE_POST_PROCESSING    = (1 << 10), // Post-processing
} rendererFeature_t;
```

### Core Functions

#### Initialization
```c
qboolean renderer->Init(void);
// Initialize the renderer
// Returns: qtrue on success, qfalse on failure

void renderer->Shutdown(void);
// Shutdown the renderer and free resources
```

#### Frame Management
```c
void renderer->BeginFrame(void);
// Begin a new frame
// Must be called before any rendering operations

void renderer->EndFrame(void);
// End the current frame
// Presents the rendered frame to the display

void renderer->Present(void);
// Present the current frame (called automatically by EndFrame)
```

#### Scene Management
```c
void renderer->BeginScene(const refdef_t* refdef);
// Begin scene rendering with the given view definition
// refdef: View parameters (position, angles, FOV, etc.)

void renderer->EndScene(void);
// End scene rendering

void renderer->ClearScene(void);
// Clear all scene data (entities, polygons, lights)
```

#### Entity Rendering
```c
void renderer->AddEntity(const refEntity_t* entity);
// Add an entity to the scene for rendering
// entity: Entity definition with model, position, animation, etc.

void renderer->AddPolygon(qhandle_t shader, int numVerts,
                         const polyVert_t* verts);
// Add a polygon to the scene for rendering
// shader: Shader handle for the polygon
// numVerts: Number of vertices
// verts: Array of vertices with position, texture coords, color
```

#### Lighting
```c
void renderer->AddLight(const dlight_t* light);
// Add a dynamic light to the scene
// light: Light definition with position, color, radius

void renderer->SetupLighting(void);
// Setup lighting for the current scene
```

#### Resource Management
```c
qhandle_t renderer->RegisterShader(const char* name);
// Register a shader for use in rendering
// name: Shader name/path
// Returns: Shader handle, 0 on failure

void renderer->RemapShader(const char* oldShader,
                          const char* newShader,
                          const char* timeOffset);
// Remap a shader to use a different implementation
// oldShader: Original shader name
// newShader: New shader name
// timeOffset: Time offset for shader animations

qhandle_t renderer->RegisterImage(const char* name);
// Register a texture/image for use in rendering
// name: Image name/path
// Returns: Image handle, 0 on failure

void renderer->UpdateImage(qhandle_t image, const void* data,
                          int x, int y, int width, int height);
// Update a portion of a texture
// image: Image handle
// data: New pixel data
// x,y,width,height: Region to update

qhandle_t renderer->RegisterModel(const char* name);
// Register a 3D model for rendering
// name: Model name/path
// Returns: Model handle, 0 on failure

qhandle_t renderer->RegisterFont(const char* fontName,
                                int pointSize,
                                fontInfo_t* font);
// Register a font for text rendering
// fontName: Font name
// pointSize: Font size in points
// font: Font information structure (output)
// Returns: Font handle, 0 on failure
```

#### Rendering Operations
```c
void renderer->RenderSurfaces(void);
// Render all surfaces in the current scene
// Called automatically during scene rendering
```

#### Post-Processing
```c
void renderer->BeginPostProcess(void);
// Begin post-processing operations

void renderer->EndPostProcess(void);
// End post-processing operations
```

#### Debug/Development
```c
void renderer->DebugDrawAxis(void);
// Draw coordinate axis for debugging

void renderer->DebugDrawNormals(void);
// Visualize surface normals

void renderer->DebugDrawTangents(void);
// Visualize surface tangents
```

#### Performance Monitoring
```c
void renderer->GetGPUInfo(char* info, int size);
// Get information about the current GPU
// info: Buffer to store GPU information
// size: Size of info buffer

void renderer->GetPerformanceStats(float* fps, float* frameTime,
                                  float* gpuTime);
// Get current performance statistics
// fps: Frames per second (output)
// frameTime: Frame time in milliseconds (output)
// gpuTime: GPU time in milliseconds (output)
```

#### Shader Management
```c
qboolean renderer->ReloadShaders(void);
// Hot-reload all shaders
// Returns: qtrue on success, qfalse on failure

qboolean renderer->ReloadTextures(void);
// Hot-reload all textures
// Returns: qtrue on success, qfalse on failure
```

#### Capability Queries
```c
qboolean renderer->HasFeature(rendererFeature_t feature);
// Check if renderer supports a specific feature
// feature: Feature to check
// Returns: qtrue if supported, qfalse otherwise

const char* renderer->GetExtensionString(void);
// Get renderer extension/capability string
// Returns: Null-terminated string of supported extensions
```

## Configuration Variables

### Renderer Selection
```
cl_renderer "vulkan"     // Renderer to use (vulkan, opengl2, opengl)
                         // Engine will fallback automatically if requested renderer fails
```

### Graphics Settings
```
r_mode "6"               // Video mode (resolution)
r_fullscreen "0"         // Fullscreen mode (0=windowed, 1=fullscreen)
r_swapInterval "0"       // V-sync (0=off, 1=on)
r_multisample "4"        // MSAA level (0,2,4,8)
r_ext_anisotropic_filtering "1"  // Anisotropic filtering
```

### Performance Settings
```
r_lodbias "0"            // Level of detail bias
r_subdivisions "4"       // Curve subdivision level
r_textureMode "GL_LINEAR_MIPMAP_LINEAR"  // Texture filtering
```

### Debug Settings
```
developer "0"            // Developer mode (0=off, 1=on)
r_showtris "0"           // Show triangle outlines
r_shownormals "0"        // Show surface normals
r_showTangentSpace "0"   // Show tangent space
```

## Error Codes

### Renderer Errors
- `RSERR_OK`: Success
- `RSERR_INVALID_MODE`: Invalid video mode
- `RSERR_FATAL_ERROR`: Fatal renderer error
- `RSERR_OLD_DRIVER`: Outdated graphics driver

### Vulkan Errors
- `VK_ERROR_OUT_OF_DEVICE_MEMORY`: GPU memory exhausted
- `VK_ERROR_OUT_OF_HOST_MEMORY`: System memory exhausted
- `VK_ERROR_DEVICE_LOST`: GPU device lost
- `VK_ERROR_SURFACE_LOST_KHR`: Display surface lost

## Best Practices

### Renderer Selection
1. Use Vulkan for best performance (automatic fallback available)
2. OpenGL2 for broad compatibility
3. Legacy OpenGL only as last resort

### Resource Management
1. Register resources at level load time
2. Reuse handles when possible
3. Clean up resources when no longer needed

### Performance Optimization
1. Use appropriate LOD settings
2. Enable anisotropic filtering for better texture quality
3. Monitor frame time with `timedemo` command

### Debugging
1. Enable `developer 1` for detailed logging
2. Use `r_showtris 1` to visualize geometry
3. Check console for renderer-specific warnings

## Platform-Specific Notes

### Linux
- Vulkan requires proper driver installation
- Wayland may need `SDL_VIDEODRIVER=x11` for compatibility
- Mesa drivers provide both Vulkan and OpenGL

### Windows
- DirectX 12 available as alternative to Vulkan
- NVIDIA/AMD drivers include Vulkan runtime
- Intel GPUs support Vulkan on recent drivers

### macOS
- Metal is the primary renderer
- Vulkan available through MoltenVK
- OpenGL deprecated on modern macOS

This API reference covers the unified renderer interface. Individual renderer implementations may have additional platform-specific features and limitations.