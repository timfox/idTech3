/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef VK_H
#define VK_H

#ifdef __cplusplus
#include <ctime>
#endif

#include <vulkan/vulkan.h>
#include "../../common/q_shared.h"  // For qboolean, vec3_t, vec4_t, byte, etc.
#include "vk_proc_dressing.h"
#include "vk_compute.h"

// VMA (Vulkan Memory Allocator) - conditional include
#ifdef USE_VMA
#include <vk_mem_alloc.h>
#endif

#include "vk_memory.h"

// ImGui forward declaration
typedef struct ImDrawData ImDrawData;

#ifdef __cplusplus
extern "C" {
#endif

// Scene management constants
#ifndef MAX_REFENTITIES
#define MAX_REFENTITIES		1024  // Match OpenGL renderer capacity
#endif
#define MAX_VERTS			16384
#define MAX_INDICES			16384

// Material parameter structure (full definition from vk_material_system.h)
typedef struct material_params_s {
	// Dynamic state
	float wetness;          // 0.0 = dry, 1.0 = fully wet
	float damage;           // 0.0 = pristine, 1.0 = destroyed
	float corruption;       // 0.0 = clean, 1.0 = corrupted
	float magicGlow;        // 0.0 = no glow, 1.0 = full glow
	float temperature;      // Temperature for thermal effects
	float time;             // Time-based animation parameter

	vec3_t magicColor;      // Magical glow color
	float _pad0;
	vec3_t damageColor;     // Damage tint color
	float _pad1;

	// Layered/PBR baseline
	vec3_t albedo;          // Base color
	vec3_t baseColor;       // Alternative base color
	float roughness;        // Surface roughness
	float metallic;         // Metallic factor
	float ao;               // Ambient occlusion
	vec3_t emissive;        // Emissive color
	float emissiveStrength; // Emissive intensity
	vec3_t normal;          // Normal map influence
	float normalScale;      // Normal scale
	float height;           // Height/displacement

	// Advanced material properties
	float subsurface;       // Subsurface scattering
	vec3_t subsurfaceColor; // Subsurface scattering color
	float microfacet;       // Microfacet intensity
	float microfacetSharpness; // Microfacet sharpness
	float layerWeight;      // Layer blending weight
	float specular;         // Specular intensity
	float specularTint;     // Specular color tint
	float anisotropic;      // Anisotropic factor
	float anisotropy;       // Anisotropy value (-1..1)
	vec3_t anisotropyDir;   // Anisotropic direction
	float sheen;            // Sheen intensity
	vec3_t sheenColor;      // Sheen color
	float sheenTint;        // Sheen color tint
	float clearcoat;        // Clearcoat intensity
	float clearcoatRoughness; // Clearcoat roughness

	// Special effects
	float iridescence;      // Iridescence factor
	float iridescenceIOR;   // Iridescence index of refraction
	float transmission;     // Transmission factor
	float ior;              // Index of refraction

	// Texture layer blending
	float layerBlend[8];    // Blend factors for up to 8 layers
	vec3_t layerTint[8];    // Tint colors for layers
	float layerOpacity[8];  // Opacity for layers

	// Runtime state
	uint32_t flags;         // Material flags
	uint32_t stateHash;     // Hash of current state for caching
	uint32_t layerCount;    // Number of active layers
	uint32_t debugFlags;    // Debug flags
	float layerCost;        // Estimated GPU cost of this stack
	float animationPhase;   // Animation phase
	float _pad2[2];         // Align to 16-byte boundary for std430
} material_params_t;

// Meshlet information structure
typedef struct {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t vertexCount;
} meshlet_info_t;

// Forward declaration for ImGui types
struct ImDrawData;

// Dear ImGui types defined in cimgui.h

// Vulkan filter constants (for compatibility)
#ifndef VK_FILTER_NEAREST_MIPMAP_NEAREST
#define VK_FILTER_NEAREST_MIPMAP_NEAREST ((VkFilter)6)
#define VK_FILTER_LINEAR_MIPMAP_NEAREST  ((VkFilter)7)
#define VK_FILTER_NEAREST_MIPMAP_LINEAR  ((VkFilter)8)
#define VK_FILTER_LINEAR_MIPMAP_LINEAR   ((VkFilter)9)
#endif

// Missing Vulkan constants
#define MAX_VK_SAMPLERS 256
#define NUM_COMMAND_BUFFERS 2
#define MAX_BINDLESS_TEXTURES 65536
#define IMAGE_CHUNK_SIZE 1024
#define MAX_ATTACHMENTS_IN_POOL 64

// Forward declarations
struct cubemap_s;
typedef struct cubemap_s cubemap_t;
struct material_params_s;
typedef struct material_params_s material_params_t;

// Forward declarations
struct vk_render_pass_s;
struct vk_pipeline_s;

// Basic Vulkan types and enums
typedef enum {
    DEPTH_RANGE_NORMAL = 0,
    DEPTH_RANGE_ZERO = 1,
    DEPTH_RANGE_ONE = 2,
    DEPTH_RANGE_WEAPON = 3,
    DEPTH_RANGE_COUNT = 4
} Vk_Depth_Range;

typedef enum {
    RENDER_PASS_MAIN = 0,
    RENDER_PASS_SCREENMAP = 1,
    RENDER_PASS_CUBEMAP = 2,
    RENDER_PASS_COUNT = 3
} renderPass_t;

// TESS flags for vertex attribute binding
#define TESS_XYZ       (1 << 0)
#define TESS_RGBA0     (1 << 1)
#define TESS_ST0       (1 << 2)
#define TESS_ST1       (1 << 3)
#define TESS_ST2       (1 << 4)
#define TESS_NNN       (1 << 5)
#define TESS_RGBA1     (1 << 6)
#define TESS_RGBA2     (1 << 7)
#define TESS_PBR       (1 << 8)
#define TESS_ENV       (1 << 9)
#define TESS_VPOS      (1 << 11)
#define TESS_ENT0      (1 << 10)

// VK_DESC descriptor binding indices
#define VK_DESC_TEXTURE_BASE           0
#define VK_DESC_UNIFORM                1
#define VK_DESC_UNIFORM_CAMERA         2
#define VK_DESC_UNIFORM_CAMERA_BINDING 2
#define VK_DESC_UNIFORM_MAIN_BINDING   3
#define VK_DESC_FOG_ONLY               4
#define VK_DESC_FOG_COLLAPSE           5
#define VK_DESC_PBR_BRDFLUT            6
#define VK_DESC_PBR_NORMAL             7
#define VK_DESC_PBR_PHYSICAL           8
#define VK_DESC_PBR_CUBEMAP            9
#define VK_DESC_MATERIAL_PARAMS        10
#define VK_DESC_FOG_DLIGHT             11
#define VK_DESC_STORAGE                12

// Bloom system constants
#define VK_NUM_BLOOM_PASSES            3

// Vulkan error checking macro
#define VK_CHECK(x) do { VkResult err = x; if (err) { ri.Error(ERR_FATAL, "Vulkan error %d at %s:%d", err, __FILE__, __LINE__); } } while (0)

// Shader file watch structure for hot reloading
typedef struct shader_file_watch_s {
    char *filename;
    time_t last_modified;
    qboolean needs_reload;
} shader_file_watch_t;

// Missing function declarations
void VK_ImGui_NotifySwapchainChanged(void);

// Cubemap filter targets
#define IRRADIANCE_TARGET              0
#define PREFILTEREDENV_TARGET          1

// Cubemap size
#define REF_CUBEMAP_SIZE               256

// PBR flags
#define PBR_HAS_NORMALMAP               (1 << 0)
#define PBR_HAS_PHYSICALMAP             (1 << 1)
#define PBR_HAS_SPECULARMAP             (1 << 2)
#define PBR_HAS_LIGHTMAP                (1 << 3)

// Physical map types
#define PHYS_NONE                       -1
#define PHYS_METALROUGH                 0
#define PHYS_SPECGLOSS                  1
#define PHYS_RMO                        2
#define PHYS_RMOS                       3
#define PHYS_MOXR                       4
#define PHYS_MOSR                       5
#define PHYS_ORM                        6
#define PHYS_ORMS                       7
#define PHYS_NORMAL                     8
#define PHYS_NORMALHEIGHT               9

// Texture map types structure
typedef struct {
    uint32_t type;
    const char* suffix;
} textureMapType_t;

// Texture map types array
static const textureMapType_t textureMapTypes[] = {
    { PHYS_METALROUGH, "_metalrough" },
    { PHYS_SPECGLOSS, "_specgloss" },
    { PHYS_RMO, "_rmo" },
    { PHYS_RMOS, "_rmos" },
    { PHYS_MOXR, "_moxr" },
    { PHYS_MOSR, "_mosr" },
    { PHYS_ORM, "_orm" },
    { PHYS_ORMS, "_orms" }
};

// Vulkan uniform structures
typedef struct {
    vec3_t eyePos;
    vec4_t ent_color[32];
    struct {
        float color[32][4];
    } ent;
    struct {
        vec4_t vector;
        vec4_t color;
        vec4_t pos;
    } light;
    float modelMatrix[16];
    float viewMatrix[16];
    float projectionMatrix[16];
    float modelViewMatrix[16];
    float modelViewProjectionMatrix[16];
    int time;
    int fogStage;
    float fogColor[4];
    float distanceFog[2];
    float distanceFogColor[4];
    float fogDistanceVector[4];
    float fogDepthVector[4];
    float fogEyeT[4];
    float lightingStage;
    float lightingBundle;
    int pbr_flags;
} vkUniform_t;

typedef struct {
    float modelMatrix[16];
    float viewOrigin[4]; // w component is 0.0
} vkUniformCamera_t;

// Command buffer structure
typedef struct {
    VkCommandBuffer command_buffer;
    VkBuffer vertex_buffer;
    VkDeviceSize vertex_buffer_offset;
    void* vertex_buffer_ptr;
    VkDeviceSize buf_offset[8];
    VkDeviceSize vbo_offset[8];
    uint32_t uniform_camera_item_size;
    uint32_t num_indexes;
    VkBuffer curr_index_buffer;
    VkDeviceSize curr_index_offset;
    Vk_Depth_Range depth_range;
    uint32_t camera_ubo_offset;

    struct {
        VkDescriptorSet current[32];
        uint32_t start;
        uint32_t end;
        uint32_t offset[32];
    } descriptor_set;

    VkRect2D scissor_rect;
    VkPipeline last_pipeline;
    VkDescriptorSet uniform_descriptor;
    uint32_t uniform_read_offset;

    struct {
        float cached_mvp[16];
        qboolean mvp_valid;
        int last_entity_num;
    } mvp_cache;

    VkDeviceSize waitForFence;

    // Frame synchronization
    VkFence rendering_finished_fence;
    VkSemaphore rendering_finished2;
    VkSemaphore image_acquired;
    qboolean swapchain_image_acquired;
    uint32_t swapchain_image_index;
} vk_cmd_t;

// World structure for Vulkan
typedef struct {
    float modelview_transform[16];
    int num_image_chunks;
    struct {
        VkDeviceMemory memory;
        VkDeviceSize used;
    } image_chunks[8];
    int dirty_depth_attachment;
} vk_world_t;

// Global world instance
extern vk_world_t vk_world;

// Vulkan pipeline definition structure
typedef struct {
    int shader_type;
    int cullType;
    int polygonOffset;
    int vk_pbr_flags;
    int fog_stage;
    int acff;
    qboolean mirror;
    int shadow_phase;
    vec4_t normalScale;
    vec4_t specularScale;
    struct {
        int rgb;
        int alpha;
    } color;
    int allow_discard;
    int state_bits;
    int face_culling;
    float line_width;
    int abs_light;
    int primitives;
} Vk_Pipeline_Def;

// Shader type constants
#define TYPE_SINGLE_TEXTURE_DF 0
#define TYPE_SINGLE_TEXTURE 1
#define TYPE_SINGLE_TEXTURE_IDENTITY 2
#define TYPE_SINGLE_TEXTURE_FIXED_COLOR 3
#define TYPE_SINGLE_TEXTURE_ENT_COLOR 4
#define TYPE_SINGLE_TEXTURE_LIGHTING 5
#define TYPE_MULTI_TEXTURE_MUL2 6
#define TYPE_MULTI_TEXTURE_MUL2_IDENTITY 7
#define TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR 8
#define TYPE_MULTI_TEXTURE_ADD2_1_1 9
#define TYPE_MULTI_TEXTURE_ADD2_IDENTITY 10
#define TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR 11
#define TYPE_MULTI_TEXTURE_ADD2 12
#define TYPE_MULTI_TEXTURE_MUL3 14
#define TYPE_MULTI_TEXTURE_MUL3_ENV 15
#define TYPE_MULTI_TEXTURE_ADD3_1_1 16
#define TYPE_MULTI_TEXTURE_ADD3 17
#define TYPE_MULTI_TEXTURE_ADD3_1_1_ENV 18
#define TYPE_MULTI_TEXTURE_ADD3_ENV 19
#define TYPE_BLEND2_MUL 20
#define TYPE_BLEND2_ADD 21
#define TYPE_BLEND2_ALPHA 22
#define TYPE_BLEND2_ONE_MINUS_ALPHA 23
#define TYPE_BLEND2_MIX_ALPHA 24
#define TYPE_BLEND2_MIX_ONE_MINUS_ALPHA 25
#define TYPE_BLEND2_DST_COLOR_SRC_ALPHA 26
#define TYPE_BLEND2_ONE_MINUS_ALPHA_ENV 27
#define TYPE_BLEND2_MIX_ALPHA_ENV 28
#define TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV 29
#define TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV 30
#define TYPE_BLEND2_ADD_ENV 31
#define TYPE_BLEND2_MUL_ENV 32
#define TYPE_BLEND2_ALPHA_ENV 33
#define TYPE_BLEND3_MUL 34
#define TYPE_BLEND3_ADD 35
#define TYPE_BLEND3_ALPHA 36
#define TYPE_BLEND3_ONE_MINUS_ALPHA 37
#define TYPE_BLEND3_MUL_ENV 38
#define TYPE_BLEND3_ALPHA_ENV 39
#define TYPE_BLEND3_ONE_MINUS_ALPHA_ENV 40
#define TYPE_BLEND3_MIX_ONE_MINUS_ALPHA 41
#define TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV 42
#define TYPE_BLEND3_MIX_ALPHA 43
#define TYPE_BLEND3_MIX_ALPHA_ENV 44
#define TYPE_BLEND3_DST_COLOR_SRC_ALPHA 45
#define TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV 46
#define TYPE_BLEND3_ADD_ENV 47
#define TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV 48
#define TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV 50
#define TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV 52
#define TYPE_MULTI_TEXTURE_MUL2_ENV 54
#define TYPE_MULTI_TEXTURE_ADD2_1_1_ENV 55
#define TYPE_MULTI_TEXTURE_ADD2_ENV 56
#define TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR 57
#define TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV 58
#define TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV 59
#define TYPE_SINGLE_TEXTURE_ENV 60
#define TYPE_SINGLE_TEXTURE_IDENTITY_ENV 61
#define TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV 62
#define TYPE_FOG_ONLY          63
// USE_FOG_ONLY is defined in tr_local.h
#define TYPE_DOT               64
#define TYPE_COLOR_RED         65
#define TYPE_COLOR_GREEN       66
#define TYPE_COLOR_BLUE        67
#define TYPE_COLOR_WHITE       68
#define TYPE_COLOR_BLACK       69
#define TYPE_GENERIC_BEGIN     71
#define TYPE_GENERIC_END       100

// Shadow phases
#define SHADOW_DISABLED  -1
#define SHADOW_EDGES     0
#define SHADOW_FS_QUAD   1

// Primitive types
#define TRIANGLE_LIST   0
#define TRIANGLE_STRIP  1
#define TRIANGLE_FAN    2
#define LINE_LIST       3
#define LINE_STRIP      4
#define POINT_LIST      5

// Vulkan constants
#define MIN_SWAPCHAIN_IMAGES_FIFO 3
#define MIN_SWAPCHAIN_IMAGES_IMM 2
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 1
#define MAX_SWAPCHAIN_IMAGES 8

// Vulkan function pointers
extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceProperties2KHR qvkGetPhysicalDeviceProperties2KHR;

// Vulkan sampler definition structure
typedef struct {
    VkFilter vk_mag_filter;
    VkFilter vk_min_filter;
    qboolean max_lod_1_0;
    qboolean noAnisotropy;
    qboolean isFontTexture;
    VkSamplerAddressMode address_mode;
} Vk_Sampler_Def;

// Stream cell structure for cell streaming
typedef struct stream_cell_s {
    int x, y, z;
    int cellX, cellY, cellZ;  // Aliases for x, y, z
    qboolean loaded;
    VkDeviceSize memoryUsed;
    int state;
    int modelCount;
    qhandle_t *models;
    void *textures;
    int textureCount;
    vec3_t worldMin;
    vec3_t worldMax;
    float priority;
    uint32_t lastAccessFrame;
} stream_cell_t;

#define MAX_STREAM_CELLS 256


// Atmosphere parameters structure
typedef struct atmosphere_params_s {
    vec3_t sun_direction;
    vec3_t sun_intensity;
    float rayleigh_scale_height;
    float mie_scale_height;
    vec3_t rayleigh_scattering;
    vec3_t mie_scattering;
    float mie_asymmetry;
    float ground_albedo;
    float atmosphere_height;
    vec3_t colorTint;
    float timeOfDay;
    vec3_t fogColor;
    float bloomIntensity;
    float bloomThreshold;
    float bloomSize;
    float exposure;
    float fogDensity;
    float fogStart;
    float fogEnd;
    uint32_t flags;
    float dofBlurRadius;
    float weatherIntensity;
    float colorTemperature;
    float dofFocusDistance;
    float brightness;
    float fogHeightFalloff;
    float contrast;
    float saturation;
} atmosphere_params_t;

// Atmosphere preset type
typedef enum {
    ATMOSPHERE_PRESET_EARTH,
    ATMOSPHERE_PRESET_MARS,
    ATMOSPHERE_PRESET_VENUS,
    ATMOSPHERE_PRESET_CUSTOM,
    ATMOSPHERE_CALM
} atmosphere_preset_t;

// Atmosphere preset constant for backward compatibility
#define ATMOSPHERE_CUSTOM ATMOSPHERE_PRESET_CUSTOM

// Descriptor count for flares
#define VK_DESC_COUNT 8

// Performance presets (defined in vk_memory.h)

// Cubemap filter definition structure
typedef struct {
    int target;
    struct {
        VkShaderModule *vs_module;
        VkShaderModule *gm_module;
        VkShaderModule *fs_module;
    } shaders;
    VkFormat format;
    uint32_t size;
    uint32_t mipLevels;
    VkRenderPass renderpass;
    struct {
        VkRenderPass renderpass;
        VkFramebuffer framebuffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkImage image;
        VkImageView view;
    } offscreen;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
} filterDef;

// Vulkan modules structure
typedef struct {
    VkShaderModule dot_vs, dot_fs;
    VkShaderModule fog_vs, fog_fs;
    VkShaderModule color_vs, color_fs;
    VkShaderModule filtercube_vs, filtercube_gm;
    VkShaderModule irradiancecube_fs, prefilterenvmap_fs;
    VkShaderModule bloom_fs, blend_fs, gamma_fs, gamma_vs, brdflut_fs;

    // Fragment shader modules (complex structure for various shader combinations)
    struct {
        VkShaderModule gen[2][2][2][2]; // Multi-dimensional array for shader variants
        VkShaderModule fixed[2][2][2]; // Fixed shader variants
        VkShaderModule ident1[2][2][2]; // Identity shader variants
        VkShaderModule ent[2][2][2]; // Entity shader variants
        VkShaderModule light[2][2]; // Light shader variants
        VkShaderModule gen0_df; // Depth fragment shader
    } frag;

    // Vertex shader modules (complex structure for various shader combinations)
    struct {
        VkShaderModule gen[2][2][2][2][2]; // Multi-dimensional array for shader variants
        VkShaderModule fixed[2][2][2][2]; // Fixed shader variants
        VkShaderModule ident1[2][2][2][2]; // Identity shader variants
        VkShaderModule light[2]; // Light vertex shader variants
    } vert;

    // Compute shader modules
    VkShaderModule velocity_tiles_comp; // Added
    VkShaderModule motion_blur_comp; // Added
    VkShaderModule color_grading_comp; // Added
    VkShaderModule heat_distortion_comp; // Added
    VkShaderModule bloom_comp; // Added
    VkShaderModule depth_of_field_comp; // Added
    VkShaderModule ssao_comp; // Added
    VkShaderModule ssr_comp; // Added
} vk_modules_t;

// Main Vulkan instance structure
typedef struct {
    qboolean active;
    VkInstance instance;

    // Bindless support
    qboolean bindless_supported;
    uint32_t bindless_texture_count;
    uint32_t bindless_buffer_count;
    uint32_t bindless_storage_buffer_count;
    uint32_t bindless_uniform_buffer_count;
    VkSampler bindless_sampler;
    float maxAnisotropy;
    VkDescriptorPool bindless_descriptor_pool;
    VkDescriptorSet bindless_descriptor_set;
    VkDescriptorSetLayout bindless_set_layout;
    VkDescriptorPool bindless_buffer_descriptor_pool;
    VkDescriptorSet bindless_buffer_descriptor_set;
    VkDescriptorSetLayout bindless_buffer_set_layout;

    // Advanced profiling systems
    vk_parallel_profiler_t parallel_profiler;

    vk_shader_performance_analyzer_t shader_performance_analyzer;

    vk_asset_loading_profiler_t asset_loading_profiler;

    vk_performance_hud_t performance_hud;

    vk_performance_regression_detector_t performance_regression_detector;

    vk_heatmap_visualizer_t heatmap_visualizer;

    vk_vram_stats_t vram_stats;

    vk_resource_pool_t resource_pools;

    vk_lock_free_memory_manager_t lock_free_manager;

    vk_arena_manager_t arena_manager;

    vk_cache_structures_manager_t cache_structures_manager;

    vk_memory_advisor_t memory_advisor;

    vk_memory_bandwidth_profiler_t memory_bandwidth_profiler;

    vk_memory_defrag_t memory_defrag;

    // Mesh shader support
    struct {
        qboolean active;
        qboolean meshShaderSupported;
        qboolean taskShaderSupported;
        qboolean useFallback;
        VkShaderModule mesh_task;
        VkShaderModule mesh_mesh;
        VkPipeline meshShaderPipeline;
        VkPipelineLayout meshShaderPipelineLayout;
        VkDescriptorSetLayout meshShaderDescriptorSetLayout;
        VkDescriptorSet meshShaderDescriptorSet;
        meshlet_info_t *meshlets;
        uint32_t meshletCount;
        uint32_t meshletCapacity;
    } mesh;

    // Performance preset
    int current_perf_preset;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family_index;
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;

    // VMA allocator (Vulkan Memory Allocator)
    VmaAllocator allocator;

    // Render passes
    struct {
        VkRenderPass main;
        VkRenderPass screenmap;
        VkRenderPass cubemap;
        VkRenderPass bloom_extract;
        VkRenderPass post_bloom;
        VkRenderPass blur[4];
        VkRenderPass capture;
        VkRenderPass brdflut;
        VkRenderPass gamma;
    } render_pass;

    // Pipelines
    VkPipelineLayout pipeline_layout;
    VkPipelineLayout pipeline_layout_storage;
    VkPipelineCache pipelineCache;
    uint32_t pipeline_create_count;
    uint32_t pipelines_world_base;
    uint32_t pipelines_count;
    struct {
        VkPipeline handle[3]; // RENDER_PASS_COUNT handles
        Vk_Pipeline_Def def;  // Pipeline definition
    } pipelines[32]; // Various pipeline types
    VkPipeline surface_debug_pipeline_solid;
    VkPipeline surface_debug_pipeline_outline;
    VkPipeline images_debug_pipeline2;
    VkPipeline tris_debug_red_pipeline;
    VkPipeline tris_debug_green_pipeline;
    VkPipeline tris_mirror_debug_red_pipeline;
    VkPipeline tris_mirror_debug_green_pipeline;
    VkPipeline tris_debug_pipeline;
    VkPipeline tris_mirror_debug_pipeline;
    VkPipeline normals_debug_pipeline;
    VkPipeline dot_pipeline;
    VkPipeline fog_pipelines[8][3][2];
    VkPipeline dlight_pipelines[2][3][2];
    VkPipeline dlight_pipelines_x[2][3][4][2];
    VkPipeline dlight1_pipelines_x[2][3][4][2];

    // Screen map
    VkSampleCountFlagBits screenMapSamples;
    struct {
        VkImage image;
        VkImageView view;
        VkImage color_image;
        VkImageView color_image_view;
        VkDescriptorSet descriptor;
        VkDescriptorSet color_descriptor;
        VkImage depth_image;
        VkImageView depth_image_view;
        VkImage color_image_msaa;
        VkImageView color_image_view_msaa;
    } screenMap;

    // Depth ranges and render pass state
    Vk_Depth_Range depth_range;
    renderPass_t renderPassIndex;

    // Memory management
    VkDeviceMemory device_memory;
    VkDeviceSize memory_size;
    void* memory_mapped;

    // Command buffers
    VkCommandBuffer command_buffer;
    vk_cmd_t* cmd;

    // Synchronization
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence fence;

    // Render state
    qboolean msaaActive;
    struct {
        VkFramebuffer screenmap;
        VkFramebuffer cubemap[6];
        VkFramebuffer brdflut;
        VkFramebuffer bloom_extract;
        VkFramebuffer blur[4];
    } framebuffers;
    uint32_t renderWidth;
    uint32_t renderHeight;
    float renderScaleX;
    float renderScaleY;
    uint32_t screenMapWidth;
    uint32_t screenMapHeight;

    // Images and views
    VkImage color_image;
    VkImageView color_image_view;
    VkDescriptorSet color_descriptor;
    VkFormat color_format;
    VkFormat capture_format;
    VkFormat bloom_format;
    qboolean blitEnabled;
    qboolean dedicatedAllocation;
    qboolean fragmentStores;
    qboolean wideLines;

    // Compute shader support
    vk_compute_manager_t compute_manager;
    VkSemaphore timeline_semaphore;

    // Atmosphere rendering
    struct {
        atmosphere_params_t baseParams;
        atmosphere_params_t targetParams;
        atmosphere_params_t currentParams;
        float transitionTime;
        float transitionDuration;
        atmosphere_preset_t currentPreset;
        qboolean enabled;
        qboolean initialized;
        VkBuffer atmosphereBuffer;
        VkDeviceMemory atmosphereBufferMemory;
    } atmosphere;

    // Debug markers support
    qboolean debugMarkers;

    // GPU culling support
    struct {
        qboolean enabled;
        qboolean initialized;
        uint32_t drawCommandCount;
        VkBuffer drawCommandBuffer;
        VkDeviceMemory drawCommandBufferMemory;
        uint32_t visibleInstanceCount;
        uint32_t instanceCount;
        uint32_t instanceCapacity;
        VkBuffer instanceBuffer;
        VkDeviceMemory instanceBufferMemory;
        VkDeviceAddress instanceBufferAddress;
        VkBuffer cullDataBuffer;
        VkDeviceMemory cullDataBufferMemory;
        VkPipeline cullPipeline;
        VkPipelineLayout cullPipelineLayout;
        VkDescriptorSet cullDescriptorSet;
    } gpuCulling;

    // Storage alignment for flare structures
    VkDeviceSize storage_alignment;

    // DLSS (Deep Learning Super Sampling) support
    struct {
        qboolean supported;
        qboolean initialized;
        uint32_t renderWidth;
        uint32_t renderHeight;
        uint32_t outputWidth;
        uint32_t outputHeight;
        uint32_t qualityMode;
        qboolean sharpeningEnabled;
        float sharpening;
        void *dlssContext;
        VkImage dlssMotionVectorImage;
        VkDeviceMemory dlssMotionVectorImageMemory;
        VkImageView dlssMotionVectorImageView;
        VkImage dlssOutputImage;
        VkDeviceMemory dlssOutputImageMemory;
        VkImageView dlssOutputImageView;
        VkImage dlssDepthImage;
        VkDeviceMemory dlssDepthImageMemory;
        VkImageView dlssDepthImageView;
    } dlss;

    // Cell streaming system
    struct {
        qboolean enabled;
        qboolean initialized;
        uint32_t cellCount;
        uint32_t activeCellCount;
        int32_t currentCellX, currentCellY, currentCellZ;
        uint32_t loadQueueCount;
        uint32_t unloadQueueCount;
        uint32_t frameCounter;
        uint32_t cellsLoadedThisFrame;
        uint32_t cellsUnloadedThisFrame;
        stream_cell_t cells[MAX_STREAM_CELLS];
        uint32_t loadQueue[MAX_STREAM_CELLS];
        uint32_t unloadQueue[MAX_STREAM_CELLS];
    } cellStreaming;
    VkDescriptorSetLayout set_layout_sampler;
    VkDescriptorSetLayout set_layout_uniform;  // Added
    VkDescriptorSetLayout set_layout_storage;  // Added
    VkDescriptorSetLayout set_layout_material; // Added

    // Bloom system
    VkPipeline bloom_extract_pipeline;
    VkPipeline bloom_blend_pipeline;
    VkPipelineLayout pipeline_layout_post_process;
    VkPipelineLayout pipeline_layout_blend;
    VkDescriptorSet bloom_image_descriptor[6];
    VkImageView bloom_image_view[6];  // Added missing bloom_image_view

    // BRDF LUT
    VkPipeline brdflut_pipeline;
    VkPipelineLayout pipeline_layout_brdflut;

    // Post-processing descriptor sets
    VkDescriptorSetLayout ssao_descriptor_layout;  // Added
    VkDescriptorSet ssao_descriptor;  // Added
    VkPipeline ssao_pipeline;  // Added
    VkDescriptorSetLayout ssr_descriptor_layout;  // Added
    VkDescriptorSet ssr_descriptor;  // Added
    VkPipeline ssr_pipeline;  // Added
    VkDescriptorSetLayout bloom_descriptor_layout;  // Added
    VkDescriptorSet bloom_descriptor;  // Added
    VkPipeline bloom_pipeline;  // Added
    VkDescriptorSetLayout dof_descriptor_layout;  // Added
    VkDescriptorSet dof_descriptor;  // Added
    VkPipeline dof_pipeline;  // Added
    VkPipelineLayout dof_layout;  // Added
    VkPipelineLayout bloom_layout;  // Added
    VkPipelineLayout ssao_layout;  // Added
    VkPipelineLayout ssr_layout;  // Added
    VkImageView coc_image_view;  // Added
    VkImageView bokeh_sprite_image_view;  // Added
    VkImageView dof_image_view;  // Added

    // Bloom system
    VkImage bloom_image[4];  // Added
    VkPipeline blur_pipeline[6];  // Added

    // Motion blur system
    VkPipeline velocity_tiles_pipeline;  // Added

    VkDescriptorSetLayout velocity_tiles_descriptor_layout;  // Added
    VkDescriptorSet velocity_tiles_descriptor;  // Added
    VkImage velocity_tiles_image;  // Added
    VkImageView velocity_image_view;  // Added
    VkImageView velocity_tiles_image_view;  // Added
    VkImageView motion_blur_image_view;  // Added
    VkPipelineLayout velocity_tiles_layout;  // Added
    VkDescriptorSetLayout motion_blur_descriptor_layout;  // Added
    VkDescriptorSet motion_blur_descriptor;  // Added
    VkPipeline motion_blur_pipeline;  // Added
    VkPipelineLayout motion_blur_layout;  // Added
    VkDescriptorSetLayout color_grading_descriptor_layout;  // Added
    VkDescriptorSet color_grading_descriptor;  // Added
    VkPipelineLayout color_grading_layout;  // Added
    VkPipeline color_grading_pipeline;  // Added
    VkImageView lut_image_view;  // Added
    VkImageView color_grading_image_view;  // Added
    VkDescriptorSetLayout heat_distortion_descriptor_layout;  // Added
    VkDescriptorSet heat_distortion_descriptor;  // Added
    VkImageView heat_distortion_image_view;  // Added
    VkPipeline heat_distortion_pipeline;  // Added
    VkPipelineLayout heat_distortion_layout;  // Added
    VkImageView heat_mask_image_view;  // Added
    VkImageView noise_image_view;  // Added

    // Cubemap system
    struct {
        VkImage color_image;
        VkImageView color_image_view;  // Added missing color_image_view
        VkDescriptorSet color_descriptor;
    } cubeMap;

    // Shader modules
    vk_modules_t modules;

    // Variable Rate Shading (VRS)
    struct {
        qboolean supported;
        qboolean enabled;
        int mode;
        float centerRadius;
        float falloffStart;
        int minRate;
        int maxRate;
    } vrs;
    VkImage vrsImage;
    VkDeviceMemory vrsImageMemory;
    VkImageView vrsImageView;
    VkDescriptorSetLayout vrsDescriptorSetLayout;
    VkDescriptorSet vrsDescriptorSet;
    VkPipeline vrs_generate_compute_pipeline;

    // Ray tracing
    qboolean rayTracingSupported;  // Added
    struct {
        qboolean initialized;
        VkDescriptorSetLayout raytracingDescriptorSetLayout;
        VkDescriptorSet raytracingDescriptorSet;
    } rt;  // Added

    // Compute pipeline
    VkPipelineLayout compute_pipeline_layout;
    VkDescriptorSet compute_descriptor_set;
    VkDescriptorSetLayout compute_descriptor_set_layout;  // Added

    // Storage buffer
    struct {
        VkBuffer buffer;  // Added missing storage buffer
        VkDeviceMemory memory;
        VkDescriptorSet descriptor;
        void* buffer_ptr;  // Added missing buffer_ptr
    } storage;

    // Performance profiling
    vk_render_profiler_t render_profiler;

    // Samplers
    struct {
        VkSampler samplers[64];
        Vk_Sampler_Def def[64];
        VkSampler handle[64];  // Added missing handle array
        int count;
        int filter_min;
        int filter_max;
        float maxLod;  // Added missing maxLod
        qboolean samplerAnisotropy;  // Added missing samplerAnisotropy
        float maxAnisotropy;  // Added missing maxAnisotropy
    } samplers;

    // Capture system
    struct {
        VkImage image;
    } capture;

    // PBR resources
    qboolean pbrActive;
    VkDescriptorSet brdflut_image_descriptor;
    struct {
        VkDescriptorSet materialDescriptorSet;
        VkBuffer materialBuffer;
        VkDeviceMemory materialBufferMemory;
        VkDeviceAddress materialBufferAddress;
        qboolean enabled;
        qboolean initialized;
        uint32_t materialCount;
        uint32_t materialCapacity;
        material_params_t *materialParams;
    } materialSystem;

    // VBO system
    struct {
        VkBuffer vertex_buffer;
        VkBuffer index_buffer;
        VkDeviceMemory memory;
        VkDeviceMemory buffer_memory;  // Added missing buffer_memory
        VkDeviceSize size;
    } vbo;

    // Staging buffer
    struct {
        VkBuffer handle;  // Added missing staging_buffer
        VkDeviceMemory memory;
        VkDeviceSize size;
        void* ptr;
        VkDeviceSize offset;
    } staging_buffer;

    // Additional state
    uint32_t uniform_alignment;
    uint32_t uniform_item_size;
    uint32_t uniform_camera_item_size;
    uint32_t camera_ubo_offset;
    VkDeviceSize vertex_buffer_offset;
    VkDeviceSize geometry_buffer_size;
    VkDeviceSize geometry_buffer_size_new;
    VkDeviceMemory geometry_buffer_memory;
    int64_t image_chunk_size;
    VkDeviceMemory image_memory[32]; // Image memory allocations
    uint32_t image_memory_count;
    struct {
        VkDeviceSize sizes[8];
        uint32_t count;
    } geometry_buffer_history;
    uint32_t maxBoundDescriptorSets;
    qboolean clearAttachment;
    VkPipeline images_debug_pipeline;
    VkPipeline shadow_finish_pipeline;
    VkPipeline shadow_volume_pipelines[2][2];
    VkPipeline skybox_pipeline;
    VkPipeline surface_beam_pipeline;
    VkPipeline surface_axis_pipeline;
    VkPipeline capture_pipeline;
    VkPipeline gamma_pipeline;
    qboolean cubemapActive;
    qboolean fboActive;
    qboolean offscreenRender;

    // Statistics
    struct {
        uint32_t push_size;
    } stats;
    struct {
        // Memory statistics
        uint32_t allocations;
        uint32_t frees;
        uint32_t current_allocations;
        VkDeviceSize total_allocated_bytes;
        VkDeviceSize total_freed_bytes;
    } vk_memory_stats;

    // Swapchain
    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkSemaphore *swapchain_rendering_finished;
    uint32_t swapchain_image_count;
    struct {
        VkFormat format;
        VkColorSpaceKHR colorSpace;
    } present_format;
    struct {
        VkFormat format;
        VkColorSpaceKHR colorSpace;
    } base_format;
    VkFormat depth_format;
    VkDescriptorSet initSwapchainLayout;

    // Profiling
    struct {
        qboolean enabled;
        uint64_t frame_start_time;
        uint64_t frame_end_time;
        double frame_time_history[128];
        uint32_t frame_time_history_index;
        double frame_time_ms;
        double frame_time_variance;
    } profiling;

    // Debug overlay
    struct {
        qboolean enabled;
        VkPipeline pipeline;
        VkDescriptorSet descriptor_set;
        float frame_time_ms;
        double frame_time_variance;
    } debug_overlay;

    // Hot reload
    struct {
        qboolean enabled;
        shader_file_watch_t* shader_file_watch_list;  // Corrected name
        uint32_t shader_file_watch_count;  // Corrected name
        qboolean pipelines_recreated;
        uint32_t shaders_reloaded;
    } hot_reload;

    // Frame management
    uint32_t frame_count;  // Added
    uint32_t cmd_index;  // Added
    vk_cmd_t tess[2];  // Added command buffer array

    // Depth image
    VkImage depth_image;       // Added
    VkImageView depth_image_view; // Added
    VkImage msaa_image;        // Added
    VkImageView msaa_image_view; // Added

    // Scene management
    struct {
        qboolean initialized;
        uint32_t entityCount;
        uint32_t polygonCount;
        refEntity_t entities[MAX_REFENTITIES];
        polyVert_t polygonVerts[MAX_VERTS];
        uint32_t polygonIndexes[MAX_INDICES];
    } scene;

    // Procedural dressing system
    struct {
        qboolean enabled;
        qboolean initialized;
        qboolean dirty;
        uint32_t biomeCount;
        uint32_t ruleCount;
        uint32_t instanceCount;
        proc_biome_t biomes[16];
        proc_rule_t rules[64];
    } procDressing;
} Vk_Instance;

// Global Vulkan instance
extern Vk_Instance vk;

// Function prototypes
void vk_begin_main_render_pass(void);
void vk_clear_depth(qboolean clear);
void vk_clear_color(const vec4_t color);
void vk_begin_cubemap_render_pass(void);
void vk_end_render_pass(void);
void vk_begin_frame(void);
void vk_end_frame(void);
void vk_bind_index(void);
void vk_bind_index_ext(uint32_t numIndexes, uint32_t* hitIndexes);
void vk_bind_pipeline(VkPipeline pipeline);
void vk_bind_geometry(uint32_t flags);
void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed);
void vk_create_image(image_t* image, int width, int height, int layers);
void vk_upload_image_data(image_t* image, int x, int y, int width, int height, int layers, const void* data, int data_size, qboolean update);
void vk_update_descriptor(uint32_t binding, VkDescriptorSet descriptor);
void vk_update_descriptor_offset(uint32_t binding, uint32_t offset);
void vk_reset_descriptor(uint32_t binding);
qboolean vk_capture_screenmap(void);
qboolean vk_clear_screenmap(void);
void vk_render_performance_hud(void);
void VK_ImGui_RenderDrawData(const ImDrawData* drawData);
uint32_t vk_push_uniform(const vkUniform_t* uniform_data);
uint32_t vk_append_uniform(const void* uniform_data, size_t size, uint32_t min_offset);
void vk_bind_lighting(int lighting_stage, int lighting_bundle);
void vk_set_depth_range(Vk_Depth_Range depth_range);
void vk_update_mvp(void* transform);
void vk_get_pipeline_def(VkPipeline pipeline, Vk_Pipeline_Def *def);
VkPipeline vk_find_pipeline_ext(int base_pipeline, Vk_Pipeline_Def* def, qboolean create_if_missing);
void vk_wait_idle(void);
void vk_queue_wait_idle(void);
void vk_update_post_process_pipelines(void);
qboolean VK_ImGui_InitBackend(void);
void VK_ImGui_ShutdownBackend(void);
void VK_ImGui_NewFrame(void);
void vk_create_brfdlut(void);
qboolean vk_capture_screenmap(void);
qboolean vk_clear_screenmap(void);
qboolean vk_bloom(void);
void vk_create_cubemap_prefilter(void);
void vk_destroy_cubemap_prefilter(void);
void vk_clear_cube_color(image_t* image, VkClearColorValue clear_color_value);
void vk_generate_cubemaps(cubemap_t* cube);
void vk_vrs_init(void);
void vk_vrs_shutdown(void);
void vk_vrs_create_resources(uint32_t width, uint32_t height);
void vk_vrs_destroy_resources(void);
void vk_vrs_generate_image(VkCommandBuffer cmdBuffer);
void vk_vrs_apply_shading_rate(VkCommandBuffer cmdBuffer);
void vk_create_prefilter_framebuffer(filterDef* def);
VkSampler vk_find_sampler(const Vk_Sampler_Def* def);
void vk_destroy_samplers(void);
void vk_update_attachment_descriptors(void);
void vk_update_descriptor_set(image_t* img, qboolean isTexture);
void vk_destroy_image_resources(VkImage* image, VkImageView* view);
float ByteToFloat(byte b);
float sRGBtoRGB(float srgb);
byte FloatToByte(float f);
void VBO_PrepareQueues(void);
void vk_shutdown_compute_manager(void);
void vk_shutdown_resource_pool(void);
void vk_clean_staging_buffer(void);
qboolean vk_allocate_image_chunk(void);
void vk_create_shader_modules(void);
void vk_shutdown_memory_pool_system(void);
void vk_shutdown_lock_free_memory_manager(void);
void vk_shutdown_arena_manager(void);
void vk_shutdown_memory_advisor(void);
void vk_shutdown_render_profiler(void);
void vk_shutdown_memory_bandwidth_profiler(void);
void vk_shutdown_parallel_profiler(void);
void vk_get_gpu_timing_stats( double *avg_frame_time_ms, double *min_frame_time_ms, double *max_frame_time_ms );
void vk_shutdown_shader_performance_analyzer(void);
void vk_shutdown_asset_loading_profiler(void);
void vk_shutdown_performance_hud(void);
void vk_shutdown_performance_regression_detector(void);
void vk_shutdown_heatmap_visualizer(void);
void vk_shutdown_cache_structures_manager(void);

// Memory tracking functions
void vk_track_allocation(VkDeviceSize size);
void vk_track_free(VkDeviceSize size);

// Missing utility functions
const char* vk_result_string(VkResult result);
VkCommandBuffer begin_command_buffer(void);
void end_command_buffer(VkCommandBuffer command_buffer, const char* function_name);

// Vulkan function pointers
extern PFN_vkCreateSampler qvkCreateSampler;
extern PFN_vkGetPhysicalDeviceFeatures2KHR qvkGetPhysicalDeviceFeatures2KHR;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkCreateImage qvkCreateImage;
extern PFN_vkGetImageMemoryRequirements qvkGetImageMemoryRequirements;
extern PFN_vkBindImageMemory qvkBindImageMemory;
extern PFN_vkCreateImageView qvkCreateImageView;
extern PFN_vkDestroyImageView qvkDestroyImageView;
extern PFN_vkDestroyImage qvkDestroyImage;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkDestroySampler qvkDestroySampler;
extern PFN_vkDestroyDescriptorPool qvkDestroyDescriptorPool;
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkMapMemory qvkMapMemory;
extern PFN_vkUnmapMemory qvkUnmapMemory;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkCmdBeginRenderPass qvkCmdBeginRenderPass;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkCmdClearColorImage qvkCmdClearColorImage;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdDrawIndexedIndirect qvkCmdDrawIndexedIndirect;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkGetBufferDeviceAddress qvkGetBufferDeviceAddress;

// Performance and debugging systems
qboolean vk_init_performance_hud(void);
qboolean vk_init_performance_regression_detector(void);
qboolean vk_init_heatmap_visualizer(void);
qboolean vk_init_compute_manager(void);
qboolean vk_init_cache_structures_manager(void);
qboolean vk_init_memory_bandwidth_profiler(void);
qboolean vk_init_parallel_profiler(void);
qboolean vk_init_shader_performance_analyzer(void);
qboolean vk_init_asset_loading_profiler(void);

// DLSS functions
void vk_dlss_destroy_resources(void);

// Drawing functions
void vk_draw_dot(uint32_t storage_offset);
void vk_bind_descriptor_sets(void);

// ImGui backend functions
VkInstance VK_GetInstanceHandle(void);
VkSampleCountFlagBits VK_GetMsaaSampleCount(void);
void VK_ImGui_RenderDrawData(const ImDrawData* drawData);

// Memory tracking functions
void vk_track_gpu_allocation(VkDeviceMemory memory, VkDeviceSize size, uint32_t memory_type, const char *resource_name, const char *allocation_site);
void vk_record_memory_access(void *address, VkDeviceSize size, const char *resource_name, qboolean is_write);
void vk_track_gpu_free(VkDeviceMemory memory);

// Utility functions
uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);
void vk_mark_pipelines_dirty(void);
void vk_bind_generated_shaders(void);
uint32_t vk_tess_index(uint32_t numIndexes, const void *src);
void vk_bind_index_buffer(VkBuffer buffer, uint32_t offset);
void vk_draw_indexed(uint32_t indexCount, uint32_t firstIndex);
qboolean create_color_attachment(uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format, VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample, VkImageCreateFlags flags);
qboolean vk_gpu_culling_is_enabled(void);

// Ray tracing functions
void vk_rt_init(void);
void vk_rt_shutdown(void);
void vk_rt_trace_rays(uint32_t width, uint32_t height);

// Memory and performance systems
void vk_init_vram_stats(void);
void vk_init_memory_defragmentation(void);
qboolean vk_init_memory_pool_system(void);
qboolean vk_init_lock_free_memory_manager(void);
qboolean vk_init_arena_manager(void);
qboolean vk_init_memory_advisor(void);
qboolean vk_init_render_profiler(void);
void vk_update_memory_pool_system(void);
void vk_reset_frame_arena(void);
void vk_update_memory_advisor(void);
void vk_profile_frame_end(void);
void vk_sample_memory_bandwidth(void);
void vk_analyze_memory_access_patterns(void);
void vk_sample_thread_utilization(void);
void vk_check_defragmentation(void);
void vk_profile_pass_start(const char* name, uint32_t drawCallCount);
void vk_profile_pass_end(const char* name, uint32_t drawCalls, uint32_t vertices);

// Scene rendering functions
void vk_render_scene(const refdef_t *fd);
void vk_clear_scene(void);
void vk_add_entity(const refEntity_t *re, qboolean intShaderTime);
void vk_add_polygon(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
void vk_set_color(const float *rgba);
void vk_draw_stretch_pic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void vk_draw_stretch_raw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);

// Shader and texture management
qhandle_t vk_register_shader(const char *name);
qhandle_t vk_register_image(const char *name, int flags);
void vk_update_image_data(image_t* image, int x, int y, int width, int height, int layers, const void* data, int data_size);

#ifdef __cplusplus
}
#endif

#endif // VK_H
