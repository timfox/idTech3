/*
=============================================================================
Terrain Rendering System
Heightmap-based terrain with LOD, material blending, and editing capabilities
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#define TERRAIN_MAX_SIZE 2048
#define TERRAIN_MAX_LOD_LEVELS 6
#define TERRAIN_PATCH_SIZE 64
#define TERRAIN_MAX_PATCHES 256
#define TERRAIN_MAX_MATERIALS 8

// Terrain material layer
typedef struct {
    qhandle_t diffuse_texture;
    qhandle_t normal_texture;
    float scale_u;
    float scale_v;
    float blend_strength;
    vec3_t tint_color;
} terrain_material_t;

// Terrain patch for LOD
typedef struct {
    int x, y;                   // Grid coordinates
    int lod_level;              // Current LOD level
    int vertex_count;
    int index_count;
    qboolean visible;           // Frustum culling
    qboolean active;            // Active in rendering

    // Vulkan resources
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;

    // Bounding box for culling
    vec3_t mins, maxs;
    float distance_to_camera;

    // Material weights (per vertex)
    VkBuffer weight_buffer;
    VkDeviceMemory weight_memory;
} terrain_patch_t;

// Terrain heightmap data
typedef struct {
    int width, height;
    float *heights;             // Height values
    vec3_t *normals;            // Surface normals
    float min_height;
    float max_height;
    float scale;                // Height scale factor
} terrain_heightmap_t;

// Terrain system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Terrain configuration
    int grid_size;              // Size of terrain grid (vertices per side)
    float patch_size_world;     // Size of each patch in world units
    float height_scale;         // Height multiplier
    int lod_distances[TERRAIN_MAX_LOD_LEVELS]; // LOD switch distances

    // Heightmap data
    terrain_heightmap_t heightmap;

    // Patches
    terrain_patch_t patches[TERRAIN_MAX_PATCHES];
    int num_patches;
    int patches_per_side;

    // Materials
    terrain_material_t materials[TERRAIN_MAX_MATERIALS];
    int num_materials;

    // Vulkan resources
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    // Heightmap texture
    VkImage height_texture;
    VkImageView height_texture_view;
    VkDeviceMemory height_texture_memory;
    VkSampler height_sampler;

    // Normal texture
    VkImage normal_texture;
    VkImageView normal_texture_view;
    VkDeviceMemory normal_texture_memory;
    VkSampler normal_sampler;

    // Material textures
    VkImage material_textures[TERRAIN_MAX_MATERIALS];
    VkImageView material_views[TERRAIN_MAX_MATERIALS];
    VkSampler material_samplers[TERRAIN_MAX_MATERIALS];

} terrain_system_t;

// External API
void vk_terrain_init(void);
void vk_terrain_shutdown(void);
void vk_terrain_update(void);
void vk_terrain_render(void);

// Terrain generation and loading
qboolean vk_terrain_load_heightmap(const char *heightmap_path, float scale);
qboolean vk_terrain_generate_heightmap(int width, int height, float scale);
void vk_terrain_set_material(int index, const char *diffuse_path, const char *normal_path,
                           float scale_u, float scale_v, const vec3_t tint_color);

// Terrain editing
void vk_terrain_set_height(int x, int y, float height);
float vk_terrain_get_height(int x, int y);
void vk_terrain_smooth_area(int x, int y, int radius);

// LOD management
void vk_terrain_update_lod(void);
void vk_terrain_set_lod_distances(int lod0_dist, int lod1_dist, int lod2_dist, int lod3_dist, int lod4_dist, int lod5_dist);

// Material blending
void vk_terrain_set_material_weight(int x, int y, int material_index, float weight);
void vk_terrain_paint_material(int x, int y, int material_index, int radius, float strength);

// Utility functions
vec3_t vk_terrain_get_normal(int x, int y);
qboolean vk_terrain_trace(const vec3_t start, const vec3_t end, vec3_t hit_pos);
void vk_terrain_get_height_range(float *min_height, float *max_height);

// CVars
extern cvar_t *r_terrain;
extern cvar_t *r_terrainLod;
extern cvar_t *r_terrainGridSize;
extern cvar_t *r_terrainPatchSize;
extern cvar_t *r_terrainMaterials;

#endif // USE_VULKAN