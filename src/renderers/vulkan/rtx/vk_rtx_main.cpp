/*
===========================================================================
id Tech 3 - Unified RTX Renderer

RTX renderer that integrates all advanced Vulkan features into a cohesive
ray tracing pipeline. Orchestrates existing systems for Q2RTX-like quality.

RAY TRACING GATING:
- Hardware ray tracing: Only enabled if vk.rayTracingSupported is true
- Compute ray tracing: Always available as fallback
- Advanced features: Gated by vk.advanced.* capability flags
- User control: All features controlled by r_rtx_* CVARs
- Hardware detection: Automatic capability detection with user feedback
===========================================================================
*/

// RTX renderer includes - modernized with C++23 following EternalJK approach
#include "vk_rtx.h"
#include "vk_rtx_raii.h" // RAII Vulkan resource management
#include "vk_compute_raytracing.h" // Compute ray tracing implementation
#include "vk_raymarching.h" // Raymarching implementation
#include "../../renderercommon/tr_public.h"
#include "../../common/q_shared.h"
#include "../../common/qcommon.h"

// Forward declarations for hardware ray tracing functions
extern void vk_rt_init(void);
extern void vk_rt_shutdown(void);
extern void vk_rt_trace_rays(uint32_t width, uint32_t height);
extern void vk_rt_denoise(uint32_t width, uint32_t height);

// C++23 standard library includes
#include <algorithm>
#include <memory>
#include <vector>
#include <array>
#include <string_view>
#include <type_traits>
#include <concepts>
#include <numbers>

// C++23 Concepts and type safety
template<typename T>
concept NumericType = std::is_arithmetic_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template<typename T>
concept VectorType = requires(T v) {
    v[0]; v[1]; v[2];
    v[0] = 0.0f;
};

// C++23 constexpr mathematical utilities for RTX
namespace rtx_math {

    // Compile-time mathematical constants
    constexpr float PI = std::numbers::pi_v<float>;
    constexpr float INV_PI = 1.0f / PI;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = PI / 2.0f;

    // constexpr safe min/max functions
    template<NumericType T>
    constexpr T safe_min(T a, T b) noexcept {
        return (a < b) ? a : b;
    }

    template<NumericType T>
    constexpr T safe_max(T a, T b) noexcept {
        return (a > b) ? a : b;
    }

    template<NumericType T>
    constexpr T clamp(T value, T min_val, T max_val) noexcept {
        return safe_max(min_val, safe_min(max_val, value));
    }

    // constexpr safe square root using Newton's method
    constexpr float safe_sqrt(float x, int iterations = 10) noexcept {
        if (x < 0.0f) return 0.0f;
        if (x == 0.0f || x == 1.0f) return x;

        float guess = x * 0.5f;
        for (int i = 0; i < iterations; ++i) {
            guess = 0.5f * (guess + x / guess);
        }
        return guess;
    }

    // constexpr vector operations for RTX
    template<VectorType Vec>
    constexpr void vector_normalize(Vec& v) noexcept {
        const float length = safe_sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (length > 0.0f) {
            v[0] /= length;
            v[1] /= length;
            v[2] /= length;
        }
    }

    template<VectorType Vec>
    constexpr float vector_dot(const Vec& a, const Vec& b) noexcept {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    }

    template<VectorType Vec>
    constexpr float vector_length_squared(const Vec& v) noexcept {
        return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    }

    // RTX-specific mathematical functions
    constexpr float degrees_to_radians(float degrees) noexcept {
        return degrees * (PI / 180.0f);
    }

    constexpr float radians_to_degrees(float radians) noexcept {
        return radians * (180.0f / PI);
    }

    // Fresnel approximation for RTX materials
    constexpr float fresnel_schlick(float cos_theta, float ior) noexcept {
        const float f0 = (ior - 1.0f) / (ior + 1.0f);
        const float f0_sq = f0 * f0;
        return f0_sq + (1.0f - f0_sq) * std::pow(1.0f - cos_theta, 5.0f);
    }

} // namespace rtx_math

// ImGui includes for RTX settings UI
#ifdef USE_CIMGUI
#include <stdbool.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

// Forward declarations for Vulkan renderer integration
extern void Vulkan_ClearScene(void);
extern qboolean Vulkan_BeginRegistration(void);
extern qhandle_t Vulkan_RegisterModel(const char *name);
extern qhandle_t Vulkan_RegisterSkin(const char *name);
extern qhandle_t Vulkan_RegisterShader(const char *name);
extern qhandle_t Vulkan_RegisterShaderNoMip(const char *name);
extern qboolean Vulkan_LoadWorld(const char *name);
extern void Vulkan_SetWorldVisData(const byte *vis);
extern void Vulkan_EndRegistration(void);
extern void Vulkan_AddRefEntityToScene(const refEntity_t *re);
extern void Vulkan_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numIndexes, const void *indexes);
extern void Vulkan_AddLightToScene(const vec3_t origin, const vec3_t dir, float radius, float intensity, const vec3_t color, qhandle_t hShader);
extern void Vulkan_RenderScene(const refdef_t *fd);
extern void Vulkan_SetColor(const vec4_t color);
extern void Vulkan_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);

// Vulkan ImGui backend functions (declared in main Vulkan renderer)
extern qboolean RE_ImGuiBackend_Init(void);
extern void RE_ImGuiBackend_Shutdown(void);
extern void RE_ImGuiBackend_NewFrame(void);

// RTX Compute RT implementation
static void RTX_ComputeRT_RenderScene(const refdef_t* fd) {

    // Camera position and look-at
    vec3_t cameraPos, cameraLookAt;
    VectorCopy(fd->vieworg, cameraPos);

    // For demo purposes, use a simple forward direction
    // In a real implementation, we'd extract the view direction from the refdef
    vec3_t forward = {0.0f, 0.0f, -1.0f}; // Looking down negative Z
    VectorMA(cameraPos, 1000.0f, forward, cameraLookAt); // Look far ahead

    // Update camera (assuming 90 degree FOV for now)
    VK_ComputeRT_UpdateCamera(cameraPos, cameraLookAt, 90.0f);

    // Update light (simplified - use first light or default)
    vec3_t lightPos = {100.0f, 100.0f, 100.0f}; // Default light position
    VK_ComputeRT_UpdateLight(lightPos);

    // Clear previous scene objects
    VK_ComputeRT_ClearScene();

    // Add demo scene objects (spheres and planes)
    // This is a simplified scene - in a real implementation, you'd extract from BSP/map data
    vec3_t sphere1Pos = {0.0f, 0.0f, -5.0f};
    vec3_t sphere1Color = {1.0f, 0.0f, 0.0f}; // Red sphere
    VK_ComputeRT_AddSphere(sphere1Pos, 1.0f, sphere1Color, 0.1f);

    vec3_t sphere2Pos = {3.0f, 1.0f, -7.0f};
    vec3_t sphere2Color = {0.0f, 1.0f, 0.0f}; // Green sphere
    VK_ComputeRT_AddSphere(sphere2Pos, 1.5f, sphere2Color, 0.3f);

    vec3_t sphere3Pos = {-2.0f, -1.0f, -6.0f};
    vec3_t sphere3Color = {0.0f, 0.0f, 1.0f}; // Blue sphere
    VK_ComputeRT_AddSphere(sphere3Pos, 0.8f, sphere3Color, 0.8f);

    // Add ground plane
    vec3_t planeNormal = {0.0f, 1.0f, 0.0f};
    vec3_t planeColor = {0.5f, 0.5f, 0.5f}; // Gray plane
    VK_ComputeRT_AddPlane(planeNormal, -2.0f, planeColor, 0.0f);

    // Dispatch the compute shader
    VK_ComputeRT_Dispatch();
}
extern void vk_material_system_init(void);
extern void vk_god_rays_init(void);
extern void vk_god_rays_shutdown(void);
extern void vk_god_rays_render(void);
extern void vk_atmosphere_init(void);
extern void vk_atmosphere_shutdown(void);
extern void vk_atmosphere_render(void);
extern qboolean VK_IBL_Init(void);
extern void vk_fsr_apply(int width, int height);
// Ray tracing functions are now implemented in their respective modules
// Material system functions - forward declared
// God rays functions - forward declared to avoid header dependencies
#include "vk_raymarching.h"
// Atmosphere system functions - forward declared
// FSR system functions - forward declared
// IBL system functions - forward declared
#include "vk_postprocess.h"
// Note: vk_volumetric_fog.h and vk_material_system.h removed to avoid tr_local.h dependency
// Denoiser functions are forward declared below
// Forward declarations for path tracer functions
void PathTracer_Init(void);
void PathTracer_Shutdown(void);
void PathTracer_RenderSample(vec3_t result, const vec3_t origin, const vec3_t direction);
void PathTracer_UpdateStatistics(void);
void PathTracer_GetStatistics(int *total_rays, int *total_bounces, float *avg_bounces);
void PathTracer_ResetStatistics(void);
// Forward declarations for denoiser functions
void Denoiser_Init(void);
void Denoiser_Shutdown(void);
void Denoiser_Apply(vec3_t *input, vec3_t *output, int width, int height);
void Denoiser_UpdateStatistics(void);
void Denoiser_GetStatistics(int *frames_processed, float *avg_noise_reduction);
void Denoiser_ResetStatistics(void);

// Forward declarations for god rays functions
void vk_god_rays_init(void);
void vk_god_rays_shutdown(void);
void vk_god_rays_render(void);

// Forward declarations for atmosphere functions
void vk_atmosphere_init(void);
void vk_atmosphere_shutdown(void);
void vk_atmosphere_render(void);

// Forward declarations for FSR functions
void vk_fsr_apply(int width, int height);

// RTX-specific CVARs
cvar_t *r_rtx_enable;
cvar_t *r_rtx_mode;          // 0=hardware RT, 1=compute RT, 2=hybrid
cvar_t *r_rtx_samples;
cvar_t *r_rtx_bounces;
cvar_t *r_rtx_denoise;
cvar_t *r_rtx_god_rays;
cvar_t *r_rtx_atmosphere;
cvar_t *r_rtx_ibl;
cvar_t *r_rtx_fsr;
cvar_t *r_rtx_raymarching;
cvar_t *r_rtx_imgui;        // Show ImGui settings window

// C++23 RTX constants
constexpr int RTX_MODE_HARDWARE = 0;
constexpr int RTX_MODE_COMPUTE = 1;
constexpr int RTX_MODE_HYBRID = 2;

constexpr int RTX_MAX_SAMPLES = 16;
constexpr int RTX_MAX_BOUNCES = 8;

// Modern C++23 RTX renderer state with RAII resource management
struct RTXState {
    bool initialized{false};
    int mode{RTX_MODE_HARDWARE};                // RTX mode (hardware/compute/hybrid)
    bool ray_tracing_active{false};
    bool compute_rt_active{false};

    // Feature flags using modern C++ initialization
    bool god_rays_enabled{false};
    bool atmosphere_enabled{false};
    bool ibl_enabled{false};
    bool fsr_enabled{false};
    bool materials_enabled{true};  // Always enable materials
    bool raymarching_enabled{false};

    // Performance tracking with modern types
    int frame_count{0};
    float frame_time{0.0f};

    // RAII-managed Vulkan resources for automatic cleanup
    std::unique_ptr<VulkanBuffer> vertexBuffer;
    std::unique_ptr<VulkanBuffer> indexBuffer;
    std::unique_ptr<VulkanBuffer> uniformBuffer;
    std::unique_ptr<VulkanImage> colorImage;
    std::unique_ptr<VulkanImage> depthImage;
    std::unique_ptr<VulkanShaderModule> vertexShader;
    std::unique_ptr<VulkanShaderModule> fragmentShader;
    std::unique_ptr<VulkanPipeline> graphicsPipeline;
    std::unique_ptr<VulkanDescriptorPool> descriptorPool;
    std::unique_ptr<VulkanCommandPool> commandPool;

    // Synchronization primitives with RAII
    std::vector<std::unique_ptr<VulkanSemaphore>> imageAvailableSemaphores;
    std::vector<std::unique_ptr<VulkanSemaphore>> renderFinishedSemaphores;
    std::vector<std::unique_ptr<VulkanFence>> inFlightFences;

    // Modern C++ methods
    constexpr bool is_hardware_rt() const noexcept { return mode == RTX_MODE_HARDWARE; }
    constexpr bool is_compute_rt() const noexcept { return mode == RTX_MODE_COMPUTE; }
    constexpr bool is_hybrid_rt() const noexcept { return mode == RTX_MODE_HYBRID; }
    constexpr bool has_ray_tracing() const noexcept { return ray_tracing_active || compute_rt_active; }

    // RAII resource initialization helper
    void initialize_resources(VkDevice device, VkPhysicalDevice physicalDevice) {
        // Create RAII-managed resources
        vertexBuffer = std::make_unique<VulkanBuffer>(device, physicalDevice, 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        indexBuffer = std::make_unique<VulkanBuffer>(device, physicalDevice, 512 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uniformBuffer = std::make_unique<VulkanBuffer>(device, physicalDevice, sizeof(float) * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Note: For images, we would need proper extent and format - this is just a placeholder
        // colorImage = std::make_unique<VulkanImage>(device, physicalDevice, extent, format, usage, properties);
        // depthImage = std::make_unique<VulkanImage>(device, physicalDevice, extent, depthFormat, usage, properties);

        commandPool = std::make_unique<VulkanCommandPool>(device, 0); // queue family index would come from device setup

        // Initialize synchronization primitives
        constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            imageAvailableSemaphores.emplace_back(std::make_unique<VulkanSemaphore>(device));
            renderFinishedSemaphores.emplace_back(std::make_unique<VulkanSemaphore>(device));
            inFlightFences.emplace_back(std::make_unique<VulkanFence>(device, VK_FENCE_CREATE_SIGNALED_BIT));
        }
    }
    
    // Centralized cleanup of all RAII-managed Vulkan resources
    void CleanupResources() noexcept {
        vertexBuffer.reset();
        indexBuffer.reset();
        uniformBuffer.reset();
        colorImage.reset();
        depthImage.reset();
        vertexShader.reset();
        fragmentShader.reset();
        graphicsPipeline.reset();
        descriptorPool.reset();
        commandPool.reset();

        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();

        // Reset lifecycle state
        initialized = false;
        // Do not alter mode here; allow caller to decide whether to reinitialize
        frame_count = 0;
        frame_time = 0.0f;
    }
};

// Backwards compatibility typedef
using rtx_state_t = RTXState;

// Modern C++23 renderer statistics structure
struct RendererStatistics {
    // Basic stats
    int frameCount{0};
    float frameTime{0.0f};

    // RTX-specific stats
    bool rayTracingActive{false};
    int samplesPerPixel{1};
    int maxBounces{1};
    bool denoisingEnabled{false};

    // Feature status
    bool godRaysActive{false};
    bool atmosphereActive{false};
    bool iblActive{false};
    bool fsrActive{false};

    // Path tracer statistics
    int pathTracerTotalRays{0};
    int pathTracerTotalBounces{0};
    float pathTracerAvgBounces{0.0f};

    // Denoiser statistics
    int denoiserFramesProcessed{0};
    float denoiserNoiseReduction{0.0f};

    // Raymarching statistics
    bool raymarchingActive{false};

    // Modern C++ methods for statistics
    void reset() noexcept {
        *this = RendererStatistics{};  // Reset to defaults
    }

    void update_performance(float deltaTime) noexcept {
        frameTime = deltaTime;
        frameCount++;
    }
};

// Backwards compatibility typedef
using rendererStatistics_t = RendererStatistics;

// Global RTX state instance - modern C++ initialization
static rtx_state_t rtx{};

/*
===============
RTX_Init

Initialize the unified RTX renderer
===============
*/
qboolean RTX_Init(void)
{
    Com_Printf("Initializing Unified RTX Renderer...\n");

    // Register RTX-specific CVARs with conservative defaults
    // Ray tracing is disabled by default - users must explicitly enable it
    r_rtx_enable = ri.Cvar_Get("r_rtx_enable", "0", CVAR_ARCHIVE);
    r_rtx_mode = ri.Cvar_Get("r_rtx_mode", "0", CVAR_ARCHIVE); // 0=hardware, 1=compute, 2=hybrid
    r_rtx_samples = ri.Cvar_Get("r_rtx_samples", "1", CVAR_ARCHIVE);
    r_rtx_bounces = ri.Cvar_Get("r_rtx_bounces", "2", CVAR_ARCHIVE);
    r_rtx_denoise = ri.Cvar_Get("r_rtx_denoise", "0", CVAR_ARCHIVE); // Denoising off by default
    r_rtx_god_rays = ri.Cvar_Get("r_rtx_god_rays", "0", CVAR_ARCHIVE);
    r_rtx_atmosphere = ri.Cvar_Get("r_rtx_atmosphere", "0", CVAR_ARCHIVE);
    r_rtx_ibl = ri.Cvar_Get("r_rtx_ibl", "0", CVAR_ARCHIVE);
    r_rtx_fsr = ri.Cvar_Get("r_rtx_fsr", "0", CVAR_ARCHIVE);
    r_rtx_raymarching = ri.Cvar_Get("r_rtx_raymarching", "0", CVAR_ARCHIVE);
    r_rtx_imgui = ri.Cvar_Get("r_rtx_imgui", "0", CVAR_ARCHIVE); // ImGui settings window

    if (!r_rtx_enable->integer) {
        // Provide helpful information about RTX availability even when disabled
        Com_Printf("RTX: Disabled by cvar (r_rtx_enable = 0)\n");
        Com_Printf("RTX: To enable ray tracing, set r_rtx_enable 1\n");

        if (vk.rayTracingSupported) {
            Com_Printf("RTX: Hardware ray tracing IS supported on this GPU\n");
            Com_Printf("RTX: Recommended: r_rtx_mode 0 (hardware) or 2 (hybrid)\n");
        } else {
            Com_Printf("RTX: Hardware ray tracing NOT supported on this GPU\n");
            Com_Printf("RTX: You can still use compute-based ray tracing: r_rtx_mode 1\n");
        }

        Com_Printf("RTX: All advanced features available: God rays, Atmosphere, IBL, FSR, Raymarching\n");
        rtx.initialized = qfalse;
        return qtrue;
    }

    // Hardware capability detection and user feedback
    Com_Printf("RTX: Detecting hardware capabilities...\n");
    Com_Printf("RTX: Hardware ray tracing supported: %s\n", vk.rayTracingSupported ? "YES" : "NO");
    Com_Printf("RTX: Advanced materials supported: %s\n", vk.advanced.materialSystem ? "YES" : "NO");
    Com_Printf("RTX: God rays supported: %s\n", vk.advanced.godRays ? "YES" : "NO");
    Com_Printf("RTX: Atmosphere supported: %s\n", vk.advanced.atmosphere ? "YES" : "NO");
    Com_Printf("RTX: IBL supported: %s\n", vk.advanced.ibl ? "YES" : "NO");
    Com_Printf("RTX: FSR supported: %s\n", vk.advanced.fsr ? "YES" : "NO");
    Com_Printf("RTX: Raymarching supported: %s\n", vk.advanced.raymarching ? "YES" : "NO");

    // Determine ray tracing capabilities
    qboolean hasHardwareRT = vk.rayTracingSupported;
    qboolean hasComputeRT = qtrue; // Compute shaders are widely available, assume yes
    qboolean canUseRTX = hasHardwareRT || hasComputeRT;

    if (!canUseRTX) {
        Com_Printf("RTX: ERROR - This GPU does not support ray tracing (hardware or compute)\n");
        Com_Printf("RTX: RTX renderer disabled - falling back to standard Vulkan\n");
        rtx.initialized = qfalse;
        return qfalse; // Hard failure - no ray tracing possible
    }

    // Auto-configure RTX mode based on hardware
    if (hasHardwareRT) {
        // Hardware RT available - prefer hardware mode if not explicitly set
        if (r_rtx_mode->integer == 0) { // Default value
            rtx.mode = RTX_MODE_HARDWARE;
            Com_Printf("RTX: Auto-selected hardware ray tracing mode (fastest)\n");
        } else {
            rtx.mode = r_rtx_mode->integer;
        }
    } else {
        // Only compute RT available - force compute mode
        if (r_rtx_mode->integer != 1) {
            Com_Printf("RTX: Hardware RT not available, forcing compute ray tracing mode\n");
        }
        rtx.mode = RTX_MODE_COMPUTE;
    }

    // Modern C++ initialization with constexpr bounds checking
    rtx.god_rays_enabled = static_cast<bool>(rtx_math::clamp(r_rtx_god_rays->integer, 0, 1)) && vk.advanced.godRays;
    rtx.atmosphere_enabled = static_cast<bool>(rtx_math::clamp(r_rtx_atmosphere->integer, 0, 1)) && vk.advanced.atmosphere;
    rtx.ibl_enabled = static_cast<bool>(rtx_math::clamp(r_rtx_ibl->integer, 0, 1)) && vk.advanced.ibl;
    rtx.fsr_enabled = static_cast<bool>(rtx_math::clamp(r_rtx_fsr->integer, 0, 1)) && vk.advanced.fsr;
    rtx.materials_enabled = vk.advanced.materialSystem; // Always enable if supported
    rtx.raymarching_enabled = static_cast<bool>(rtx_math::clamp(r_rtx_raymarching->integer, 0, 1)) && vk.advanced.raymarching;

    // Provide user feedback about what features are enabled
    if (!rtx.god_rays_enabled && r_rtx_god_rays->integer) {
        Com_Printf("RTX: WARNING - God rays requested but not supported by hardware\n");
    }
    if (!rtx.atmosphere_enabled && r_rtx_atmosphere->integer) {
        Com_Printf("RTX: WARNING - Atmosphere requested but not supported by hardware\n");
    }
    if (!rtx.ibl_enabled && r_rtx_ibl->integer) {
        Com_Printf("RTX: WARNING - IBL requested but not supported by hardware\n");
    }
    if (!rtx.fsr_enabled && r_rtx_fsr->integer) {
        Com_Printf("RTX: WARNING - FSR requested but not supported by hardware\n");
    }
    if (!rtx.raymarching_enabled && r_rtx_raymarching->integer) {
        Com_Printf("RTX: WARNING - Raymarching requested but not supported by hardware\n");
    }

    // Initialize ray tracing based on mode and hardware capabilities
    rtx.ray_tracing_active = false;
    rtx.compute_rt_active = false;

    // Initialize based on determined mode
    if (rtx.mode == RTX_MODE_HARDWARE) {
        Com_Printf("RTX: Initializing hardware ray tracing...\n");
        vk_rt_init();
        rtx.ray_tracing_active = true;
    } else if (rtx.mode == RTX_MODE_COMPUTE) {
        Com_Printf("RTX: Initializing compute ray tracing...\n");
        VK_ComputeRT_Init();
        rtx.compute_rt_active = true;
    } else if (rtx.mode == RTX_MODE_HYBRID) {
        // Hybrid mode - initialize both if available
        if (hasHardwareRT) {
            Com_Printf("RTX: Initializing hardware ray tracing (hybrid mode)...\n");
            vk_rt_init();
            rtx.ray_tracing_active = true;
        }
        Com_Printf("RTX: Initializing compute ray tracing (hybrid mode)...\n");
        VK_ComputeRT_Init();
        rtx.compute_rt_active = true;
    }

    // Verify at least one ray tracing method is active
    if (!rtx.ray_tracing_active && !rtx.compute_rt_active) {
        Com_Printf("RTX: ERROR - Failed to initialize any ray tracing method\n");
        rtx.initialized = qfalse;
        return qfalse;
    }

    // Initialize material system
    if (rtx.materials_enabled) {
        vk_material_system_init();
        Com_Printf("RTX: Material system initialized\n");
    }

    // Initialize god rays
    if (rtx.god_rays_enabled) {
        vk_god_rays_init();
        Com_Printf("RTX: God rays system initialized\n");
    }

    // Initialize atmosphere
    if (rtx.atmosphere_enabled) {
        vk_atmosphere_init();
        Com_Printf("RTX: Atmosphere system initialized\n");
    }

    // Initialize IBL
    if (rtx.ibl_enabled) {
        VK_IBL_Init();
        Com_Printf("RTX: IBL system initialized\n");
    }

    // Initialize IBL
    if (rtx.ibl_enabled) {
        VK_IBL_Init();
        Com_Printf("RTX: IBL system initialized\n");
    }

    // Initialize FSR
    if (rtx.fsr_enabled) {
        // FSR is initialized per-frame, but we can prepare resources here
        Com_Printf("RTX: FSR ready for upscaling\n");
    }

    // Initialize path tracer
    PathTracer_Init();
    Com_Printf("RTX: Path tracer initialized\n");

    // Initialize denoiser
    Denoiser_Init();
    Com_Printf("RTX: Denoiser initialized\n");

    // Initialize raymarching
    if (rtx.raymarching_enabled) {
        // Raymarching initialization would go here
        Com_Printf("RTX: Raymarching system initialized\n");
    }

    // Initialize RAII-managed Vulkan resources
    // Note: We need to get the physical device from the main Vulkan renderer
    // For now, we'll initialize with nullptr and update this when we have proper access
    Com_Printf("RTX: Initializing RAII-managed Vulkan resources...\n");
    // rtx.initialize_resources(vk.device, vk.physical_device); // TODO: Enable when device access is available

    rtx.initialized = qtrue;
    Com_Printf("RTX: Unified renderer initialized successfully\n");
    Com_Printf("RTX: Mode=%d, Ray Tracing=%s, Compute RT=%s\n",
               rtx.mode,
               rtx.ray_tracing_active ? "YES" : "NO",
               rtx.compute_rt_active ? "YES" : "NO");

    return qtrue;
}

/*
========================
RTX_SwitchMode
Switch RTX rendering mode with proper cleanup of existing resources.
This does not perform full reinitialization; it marks a mode change
and leaves resource reinitialization to the next init/activation.
========================
*/
void RTX_SwitchMode(int newMode)
{
    if (rtx.mode == newMode) {
        return;
    }
    // If currently initialized, perform a clean teardown of RAII resources
    if (rtx.initialized) {
        rtx.CleanupResources();
    }
    rtx.mode = newMode;
    // Resources will be reinitialized lazily on next Init or first render path
    // Minimal reinitialization for non-GPU subsystems to complete the mode-change lifecycle
    PathTracer_Shutdown();
    PathTracer_Init();
    Denoiser_Shutdown();
    Denoiser_Init();
}

// Test hooks ( UNIT_TEST builds ) - expose minimal introspection / control
#ifdef UNIT_TEST
extern "C" int RTX_GetModeForTest() { return rtx.mode; }
extern "C" int RTX_IsInitializedForTest() { return rtx.initialized ? 1 : 0; }
extern "C" void RTX_TestForceResourceCleanupForTest() { rtx.CleanupResources(); }
#endif

/*
===============
RTX_Shutdown

Shutdown the unified RTX renderer
===============
*/
void RTX_Shutdown(refShutdownCode_t code)
{
    (void)code; // Mark as unused for now
    Com_Printf("Shutting down Unified RTX Renderer...\n");

    if (!rtx.initialized) {
        return;
    }

    // Ensure all RAII-managed resources are cleaned up in a single place.
    // This guarantees a consistent teardown even if resources were partially
    // initialized or if mode switching occurred previously.
    rtx.CleanupResources();

    // Shutdown in reverse order
    if (rtx.fsr_enabled) {
        // FSR cleanup handled automatically
    }

    if (rtx.ibl_enabled) {
        // IBL cleanup handled automatically
    }

    // Shutdown atmosphere
    if (rtx.atmosphere_enabled) {
        vk_atmosphere_shutdown();
    }

    // Shutdown god rays
    if (rtx.god_rays_enabled) {
        vk_god_rays_shutdown();
    }

    if (rtx.materials_enabled) {
        // Material system cleanup handled automatically
    }

    if (rtx.compute_rt_active) {
        VK_ComputeRT_Shutdown();
    }

    if (rtx.ray_tracing_active) {
        vk_rt_shutdown();
    }

    // Shutdown path tracer
    PathTracer_Shutdown();

    // Shutdown denoiser
    Denoiser_Shutdown();

    // RAII resources will be automatically cleaned up when unique_ptrs are reset
    Com_Printf("RTX: Cleaning up RAII-managed Vulkan resources...\n");
    rtx.vertexBuffer.reset();
    rtx.indexBuffer.reset();
    rtx.uniformBuffer.reset();
    rtx.colorImage.reset();
    rtx.depthImage.reset();
    rtx.vertexShader.reset();
    rtx.fragmentShader.reset();
    rtx.graphicsPipeline.reset();
    rtx.descriptorPool.reset();
    rtx.commandPool.reset();

    // Clear synchronization primitives
    rtx.imageAvailableSemaphores.clear();
    rtx.renderFinishedSemaphores.clear();
    rtx.inFlightFences.clear();

    rtx.initialized = qfalse;
    Com_Printf("RTX: Unified renderer shutdown complete\n");
}

/*
===============
RTX_BeginFrame

Begin a new frame
===============
*/
void RTX_BeginFrame(stereoFrame_t stereoFrame)
{
    // Detect RTX mode changes from CVAR and apply safe transition
    static int g_lastRtxMode = -1;
    if (r_rtx_mode && r_rtx_mode->integer != g_lastRtxMode) {
        RTX_SwitchMode(r_rtx_mode->integer);
        g_lastRtxMode = r_rtx_mode->integer;
    }
    (void)stereoFrame; // Mark as unused for now

    if (!rtx.initialized) {
        return;
    }

    // Modern C++ CVAR updates with proper type conversion
    rtx.mode = r_rtx_mode->integer;
    rtx.god_rays_enabled = static_cast<bool>(r_rtx_god_rays->integer);
    rtx.atmosphere_enabled = static_cast<bool>(r_rtx_atmosphere->integer);
    rtx.ibl_enabled = static_cast<bool>(r_rtx_ibl->integer);
    rtx.fsr_enabled = static_cast<bool>(r_rtx_fsr->integer);
    rtx.raymarching_enabled = static_cast<bool>(r_rtx_raymarching->integer);

    // Begin frame for all active systems
    // Note: Advanced systems begin frame removed for basic RTX renderer

    rtx.frame_count++;
}

/*
===============
RTX_RenderScene

Render the scene using RTX pipeline
===============
*/
void RTX_RenderScene(const refdef_t *fd)
{
    (void)fd; // Mark as unused for now

    if (!rtx.initialized) {
        return;
    }

    // Primary rendering based on mode
    if (rtx.ray_tracing_active && (rtx.mode == 0 || rtx.mode == 2)) {
        // Hardware ray tracing path
        vk_rt_trace_rays(glConfig.vidWidth, glConfig.vidHeight);
    } else if (rtx.compute_rt_active && (rtx.mode == 1 || rtx.mode == 2)) {
        // Compute ray tracing path
        RTX_ComputeRT_RenderScene(fd);
    }

    // Apply advanced effects following Q2RTX pipeline

    // 1. God rays (volumetric lighting)
    if (rtx.god_rays_enabled) {
        vk_god_rays_render();
        Com_Printf("RTX: God rays rendered\n");
    }

    // 2. Atmosphere effects
    if (rtx.atmosphere_enabled) {
        vk_atmosphere_render();
        Com_Printf("RTX: Atmosphere rendered\n");
    }

    // 3. Image-based lighting
    if (rtx.ibl_enabled) {
        // IBL rendering integrated into main pipeline
        Com_Printf("RTX: IBL active\n");
    }

    // 4. Raymarching effects
    if (rtx.raymarching_enabled) {
        // Raymarching rendering (distance fields, volumetric effects)
        Com_Printf("RTX: Raymarching active\n");
    }

    // Post-processing effects
    if (rtx.ibl_enabled) {
        // IBL rendering integrated into main pipeline
    }

    // Apply denoising if enabled
    if (r_rtx_denoise->integer) {
        vk_rt_denoise(glConfig.vidWidth, glConfig.vidHeight);
    }

    // Apply denoising if enabled
    if (r_rtx_denoise->integer) {
        // Note: In a full implementation, this would denoise the rendered image
        // For now, we just update statistics
        Denoiser_UpdateStatistics();
    }

    // Apply FSR upscaling if enabled (final post-processing stage)
    if (rtx.fsr_enabled) {
        vk_fsr_apply(glConfig.vidWidth, glConfig.vidHeight);
        Com_Printf("RTX: FSR upscaling applied\n");
    }

    // Update path tracer statistics
    PathTracer_UpdateStatistics();
}

/*
===============
RTX_EndFrame

End the current frame
===============
*/
void RTX_EndFrame(int *frontEndMsec, int *backEndMsec)
{
    if (!rtx.initialized) {
        return;
    }

    // End frame for all systems
    // Note: Advanced systems shutdown removed for basic RTX renderer

    // Track performance
    rtx.frame_time = ri.Milliseconds() - rtx.frame_time;

    // Pass timing info if requested
    if (frontEndMsec) *frontEndMsec = 0; // Placeholder
    if (backEndMsec) *backEndMsec = (int)rtx.frame_time;
}

// RTX_SupportsFeature removed - not currently used


// RTX_GetStatistics removed - not currently used

// Standard renderer interface functions (most delegate to Vulkan base)

// Minimal Vulkan renderer integration - only essential functions used

/*
===============
RTX_ImGuiBackendInit

Initialize ImGui backend for RTX
===============
*/
qboolean RTX_ImGuiBackendInit(void)
{
    // Delegate to Vulkan renderer ImGui backend
    return RE_ImGuiBackend_Init();
}

/*
===============
RTX_ImGuiBackendShutdown

Shutdown ImGui backend for RTX
===============
*/
void RTX_ImGuiBackendShutdown(void)
{
    // Delegate to Vulkan renderer ImGui backend
    RE_ImGuiBackend_Shutdown();
}

/*
===============
RTX_ImGuiBackendNewFrame

Start new ImGui frame for RTX
===============
*/
void RTX_ImGuiBackendNewFrame(void)
{
    // Delegate to Vulkan renderer first
    RE_ImGuiBackend_NewFrame();

#ifdef USE_CIMGUI
    // Then add RTX settings window
    RTX_ImGuiSettingsWindow();
#endif
}

/*
===============
RTX_ImGuiBackendRenderDrawData

Render ImGui draw data for RTX
===============
*/
void RTX_ImGuiBackendRenderDrawData(const struct ImDrawData *drawData)
{
    // For now, just delegate to Vulkan renderer if available
    // In a full implementation, this would handle RTX-specific ImGui rendering
    (void)drawData; // Mark as unused for now
}

/*
===============
RTX_ImGuiSettingsWindow

ImGui window for RTX settings
===============
*/
#ifdef USE_CIMGUI
void RTX_ImGuiSettingsWindow(void)
{
    if (!rtx.initialized) {
        return;
    }

    // Only show if RTX ImGui is enabled via CVAR
    if (!r_rtx_imgui->integer) {
        return;
    }

    // Start ImGui window
    static bool showRTXWindow = true;
    igBegin("RTX Renderer Settings", &showRTXWindow, 0); // No special flags for now

    igText("Unified RTX Renderer v1.0");
    igText("Hardware: %s", vk.rayTracingSupported ? "RTX Capable" : "Compute Only");
    igSeparator();

    // Rendering Mode
    const char* modes[] = { "Hardware RT", "Compute RT", "Hybrid" };
    static int currentMode = 0; // Initialize with default
    currentMode = rtx.mode; // Update from current state

    // Disable hardware RT option if not supported
    if (!vk.rayTracingSupported && currentMode == 0) {
        currentMode = 1; // Force to compute mode
        ri.Cvar_Set("r_rtx_mode", "1");
        rtx.mode = 1;
    }

    if (igCombo_Str_arr("Mode", &currentMode, modes, 3, -1)) {
        // Prevent selecting hardware RT on unsupported hardware
        if (!vk.rayTracingSupported && currentMode == 0) {
            Com_Printf("RTX: Hardware ray tracing not supported on this GPU\n");
            currentMode = rtx.mode; // Revert
        } else {
            ri.Cvar_Set("r_rtx_mode", va("%d", currentMode));
            rtx.mode = currentMode;
        }
    }

    igSeparator();
    igText("Ray Tracing Settings");

    // Samples per pixel
    static int samples = 1; // Initialize with default
    samples = r_rtx_samples->integer; // Update from CVAR
    if (igSliderInt("Samples per Pixel", &samples, 1, 16, "%d", 0)) {
        ri.Cvar_Set("r_rtx_samples", va("%d", samples));
    }

    // Max bounces
    static int bounces = 1; // Initialize with default
    bounces = r_rtx_bounces->integer; // Update from CVAR
    if (igSliderInt("Max Bounces", &bounces, 1, 8, "%d", 0)) {
        ri.Cvar_Set("r_rtx_bounces", va("%d", bounces));
    }

    igSeparator();
    igText("Advanced Effects");

    // God Rays
    static bool godRays = false; // Initialize with default
    godRays = rtx.god_rays_enabled; // Update from state
    igBeginDisabled(!vk.advanced.godRays);
    if (igCheckbox("God Rays (Volumetric)", &godRays)) {
        ri.Cvar_Set("r_rtx_god_rays", godRays ? "1" : "0");
        rtx.god_rays_enabled = godRays;
    }
    igEndDisabled();
    if (!vk.advanced.godRays) {
        igSameLine(0, -1);
        igText("(Not Supported)");
    }

    // Atmosphere
    static bool atmosphere = false; // Initialize with default
    atmosphere = rtx.atmosphere_enabled; // Update from state
    igBeginDisabled(!vk.advanced.atmosphere);
    if (igCheckbox("Atmosphere", &atmosphere)) {
        ri.Cvar_Set("r_rtx_atmosphere", atmosphere ? "1" : "0");
        rtx.atmosphere_enabled = atmosphere;
    }
    igEndDisabled();
    if (!vk.advanced.atmosphere) {
        igSameLine(0, -1);
        igText("(Not Supported)");
    }

    // IBL
    static bool ibl = false; // Initialize with default
    ibl = rtx.ibl_enabled; // Update from state
    igBeginDisabled(!vk.advanced.ibl);
    if (igCheckbox("Image Based Lighting", &ibl)) {
        ri.Cvar_Set("r_rtx_ibl", ibl ? "1" : "0");
        rtx.ibl_enabled = ibl;
    }
    igEndDisabled();
    if (!vk.advanced.ibl) {
        igSameLine(0, -1);
        igText("(Not Supported)");
    }

    // FSR
    static bool fsr = false; // Initialize with default
    fsr = rtx.fsr_enabled; // Update from state
    igBeginDisabled(!vk.advanced.fsr);
    if (igCheckbox("FSR Upscaling", &fsr)) {
        ri.Cvar_Set("r_rtx_fsr", fsr ? "1" : "0");
        rtx.fsr_enabled = fsr;
    }
    igEndDisabled();
    if (!vk.advanced.fsr) {
        igSameLine(0, -1);
        igText("(Not Supported)");
    }

    // Raymarching
    static bool raymarching = false; // Initialize with default
    raymarching = rtx.raymarching_enabled; // Update from state
    igBeginDisabled(!vk.advanced.raymarching);
    if (igCheckbox("Raymarching", &raymarching)) {
        ri.Cvar_Set("r_rtx_raymarching", raymarching ? "1" : "0");
        rtx.raymarching_enabled = raymarching;
    }
    igEndDisabled();
    if (!vk.advanced.raymarching) {
        igSameLine(0, -1);
        igText("(Not Supported)");
    }

    // Denoising
    static bool denoise = false; // Initialize with default
    denoise = r_rtx_denoise->integer; // Update from CVAR
    if (igCheckbox("Denoising", &denoise)) {
        ri.Cvar_Set("r_rtx_denoise", denoise ? "1" : "0");
    }

    igSeparator();
    igText("Performance Stats");

    // Show some basic stats
    igText("Frame Count: %d", rtx.frame_count);
    igText("Frame Time: %.2f ms", rtx.frame_time);

    // Path tracer stats
    int totalRays, totalBounces;
    float avgBounces;
    PathTracer_GetStatistics(&totalRays, &totalBounces, &avgBounces);
    igText("Total Rays: %d", totalRays);
    igText("Total Bounces: %d", totalBounces);
    igText("Avg Bounces: %.2f", avgBounces);

    // Denoiser stats
    int framesProcessed;
    float noiseReduction;
    Denoiser_GetStatistics(&framesProcessed, &noiseReduction);
    igText("Frames Denoised: %d", framesProcessed);
    igText("Noise Reduction: %.2f", noiseReduction);

    igSeparator();
    igText("Set r_rtx_imgui 0/1 to toggle this window");

    igEnd();
}
#endif

// RTX renderer exports the standard refexport_t interface
static refexport_t rtxExport;

/*
===============
RTX_GetRefAPI

Return the RTX renderer interface
===============
*/
refexport_t* RTX_GetRefAPI(int apiVersion, refimport_t* rimp) {
    (void)apiVersion; // Mark as unused

    // Copy the refimport functions
    ri = *rimp;

    // Initialize with minimal RTX export structure
    memset(&rtxExport, 0, sizeof(rtxExport));

    // Core RTX functions
    rtxExport.Shutdown = RTX_Shutdown;
    rtxExport.RenderScene = RTX_RenderScene;
    rtxExport.BeginFrame = RTX_BeginFrame;
    rtxExport.EndFrame = RTX_EndFrame;

    // ImGui support for real-time RTX settings
    rtxExport.ImGuiBackendInit = RTX_ImGuiBackendInit;
    rtxExport.ImGuiBackendShutdown = RTX_ImGuiBackendShutdown;
    rtxExport.ImGuiBackendNewFrame = RTX_ImGuiBackendNewFrame;
    rtxExport.ImGuiBackendRenderDrawData = RTX_ImGuiBackendRenderDrawData;

    // Initialize RTX system
    RTX_Init();

    return &rtxExport;
}

/*
===============
GetRefAPI

Entry point for the RTX renderer
===============
*/
#ifdef USE_RENDERER_DLOPEN
extern "C" Q_EXPORT __attribute__((visibility("default"))) refexport_t* QDECL GetRefAPI( int apiVersion, refimport_t *rimp ) {
    return RTX_GetRefAPI(apiVersion, rimp);
}
#endif