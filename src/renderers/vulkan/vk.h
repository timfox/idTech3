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

#include <vulkan/vulkan.h>

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

    struct {
        float cached_mvp[16];
        qboolean mvp_valid;
        int last_entity_num;
    } mvp_cache;

    VkDeviceSize waitForFence;
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
#define TYPE_MULTI_TEXTURE_MUL2 5
#define TYPE_MULTI_TEXTURE_MUL2_IDENTITY 6
#define TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR 7
#define TYPE_MULTI_TEXTURE_ADD2_1_1 8
#define TYPE_MULTI_TEXTURE_ADD2_IDENTITY 9
#define TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR 10
#define TYPE_MULTI_TEXTURE_ADD2 11
#define TYPE_MULTI_TEXTURE_MUL3 14
#define TYPE_MULTI_TEXTURE_ADD3_1_1 15
#define TYPE_MULTI_TEXTURE_ADD3 16
#define TYPE_BLEND2_MUL 17
#define TYPE_BLEND2_ADD 18
#define TYPE_BLEND2_ALPHA 19
#define TYPE_BLEND2_ONE_MINUS_ALPHA 20
#define TYPE_BLEND2_MIX_ALPHA 21
#define TYPE_BLEND2_MIX_ONE_MINUS_ALPHA 22
#define TYPE_BLEND2_DST_COLOR_SRC_ALPHA 23
#define TYPE_BLEND3_MUL 24
#define TYPE_BLEND3_ADD 25
#define TYPE_BLEND3_ALPHA 26
#define TYPE_BLEND3_ONE_MINUS_ALPHA 27
#define TYPE_BLEND3_MIX_ONE_MINUS_ALPHA 28
#define TYPE_BLEND3_MIX_ALPHA 29
#define TYPE_BLEND3_DST_COLOR_SRC_ALPHA 30
#define TYPE_GENERIC_BEGIN     31
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

// Vulkan sampler definition structure
typedef struct {
    VkFilter vk_mag_filter;
    VkFilter vk_min_filter;
    qboolean max_lod_1_0;
    qboolean noAnisotropy;
    VkSamplerAddressMode address_mode;
} Vk_Sampler_Def;

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
    VkShaderModule bloom_fs, blend_fs, gamma_fs;

    // Fragment shader modules (complex structure for various shader combinations)
    struct {
        VkShaderModule gen[2][2][2][2]; // Multi-dimensional array for shader variants
        VkShaderModule fixed[2][2][2]; // Fixed shader variants
        VkShaderModule ident1[2][2][2]; // Identity shader variants
        VkShaderModule ent[2][2][2]; // Entity shader variants
    } frag;

    // Vertex shader modules (complex structure for various shader combinations)
    struct {
        VkShaderModule gen[2][2][2][2][2]; // Multi-dimensional array for shader variants
        VkShaderModule fixed[2][2][2][2]; // Fixed shader variants
        VkShaderModule ident1[2][2][2][2]; // Identity shader variants
    } vert;
} vk_modules_t;

// Main Vulkan instance structure
typedef struct {
    qboolean active;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family_index;
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;

    // Render passes
    struct {
        VkRenderPass main;
        VkRenderPass screenmap;
        VkRenderPass cubemap;
        VkRenderPass bloom_extract;
        VkRenderPass post_bloom;
        VkRenderPass capture;
        VkRenderPass brdflut;
        VkRenderPass gamma;
    } render_pass;

    // Pipelines
    VkPipelineLayout pipeline_layout;
    VkPipelineLayout pipeline_layout_storage;
    VkPipelineCache pipelineCache;
    uint32_t pipeline_create_count;
    VkPipeline pipelines[32]; // Various pipeline types
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
    VkDescriptorSetLayout set_layout_sampler;

    // Bloom system
    VkPipeline bloom_extract_pipeline;
    VkPipeline bloom_blend_pipeline;
    VkPipelineLayout pipeline_layout_post_process;
    VkPipelineLayout pipeline_layout_blend;
    VkDescriptorSet bloom_image_descriptor[6];

    // BRDF LUT
    VkPipeline brdflut_pipeline;
    VkPipelineLayout pipeline_layout_brdflut;

    // Cubemap system
    struct {
        VkImage color_image;
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

    // Compute pipeline
    VkPipelineLayout compute_pipeline_layout;
    VkDescriptorSet compute_descriptor_set;

    // Performance profiling
    struct {
        uint32_t timestamp_queries[2];
    } render_profiler;

    // Samplers
    struct {
        VkSampler samplers[64];
        Vk_Sampler_Def def[64];
        int count;
        int filter_min;
        int filter_max;
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
        qboolean enabled;
        qboolean initialized;
    } materialSystem;

    // VBO system
    struct {
        VkBuffer vertex_buffer;
        VkBuffer index_buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
    } vbo;

    // Additional state
    uint32_t uniform_alignment;
    uint32_t uniform_item_size;
    uint32_t uniform_camera_item_size;
    uint32_t camera_ubo_offset;
    VkDeviceSize vertex_buffer_offset;
    VkDeviceSize geometry_buffer_size;
    VkDeviceSize geometry_buffer_size_new;
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
    VkPipeline blur_pipeline;
    qboolean cubemapActive;
    qboolean fboActive;
    qboolean offscreenRender;

    // Statistics
    struct {
        uint32_t push_size;
    } stats;
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
Vk_Pipeline_Def* vk_get_pipeline_def(VkPipeline pipeline, Vk_Pipeline_Def* def);
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

#endif // VK_H
