/*
=============================================================================
Decals System
Dynamic decal rendering for bullet holes, scorch marks, and surface effects
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#define MAX_DECALS 1024
#define MAX_DECAL_VERTICES 4096
#define MAX_DECAL_INDICES 6144

// Decal types
typedef enum {
    DECAL_BULLET_HOLE,
    DECAL_SCORCH_MARK,
    DECAL_BLOOD_SPLATTER,
    DECAL_EXPLOSION,
    DECAL_CUSTOM
} decal_type_t;

// Decal structure
typedef struct {
    qboolean active;
    decal_type_t type;

    vec3_t position;
    vec3_t normal;
    vec3_t tangent;
    vec3_t binormal;

    float radius;
    float angle;
    float lifetime;        // Total lifetime in seconds
    float fade_time;       // Fade out time at end of life
    float start_time;      // When the decal was created

    vec4_t color;          // RGBA color tint
    float alpha;           // Current alpha value

    int material_index;    // Texture/material index
    qboolean fade_out;     // Whether to fade out over time

    // Rendering data
    int vertex_offset;
    int index_offset;
    int vertex_count;
    int index_count;

    // Spatial partitioning
    int cluster;           // BSP cluster for culling
    vec3_t mins, maxs;     // Bounding box
} decal_t;

// Decal material definition
typedef struct {
    char name[MAX_QPATH];
    qhandle_t shader;
    qboolean animated;
    float frame_time;
    int num_frames;
} decal_material_t;

// Decal system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Decal storage
    decal_t decals[MAX_DECALS];
    int num_decals;
    int next_decal_index;

    // Rendering resources
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;

    // Pipeline
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    // Materials
    decal_material_t materials[32];
    int num_materials;

    // Vertex data (for batching)
    vec4_t *vertices;       // position, texcoord
    uint32_t *indices;
    int vertex_count;
    int index_count;

} decal_system_t;

// External API
void vk_decals_init(void);
void vk_decals_shutdown(void);
void vk_decals_update(void);
void vk_decals_render(void);

// Decal management
int vk_decals_create_decal(decal_type_t type, const vec3_t position, const vec3_t normal,
                          float radius, float angle, float lifetime, const vec4_t color);
void vk_decals_remove_decal(int decal_index);
void vk_decals_clear_all(void);

// Material management
int vk_decals_register_material(const char *name, qhandle_t shader, qboolean animated,
                               float frame_time, int num_frames);
qhandle_t vk_decals_get_material_shader(int material_index);

// Utility functions
void vk_decals_project_on_surface(const vec3_t start, const vec3_t end, vec3_t position, vec3_t normal);
qboolean vk_decals_trace_surface(const vec3_t start, const vec3_t end, vec3_t position, vec3_t normal);

// CVars
extern cvar_t *r_decals;
extern cvar_t *r_decalsMax;
extern cvar_t *r_decalsFadeTime;

#endif // USE_VULKAN