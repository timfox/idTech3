/*
=============================================================================
Surface Sprites System
Particle-like effects attached to surfaces (grass, debris, flowers, etc.)
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

#define MAX_SURFACE_SPRITE_TYPES 64
#define MAX_SURFACE_SPRITES 8192
#define SURFACE_SPRITE_BATCH_SIZE 1024

// Surface sprite type definition
typedef struct {
    char name[MAX_QPATH];
    qhandle_t texture;
    qboolean animated;
    int num_frames;
    float frame_time;
    vec2_t size;              // Width and height
    vec3_t color;             // Base color tint
    float alpha;              // Base alpha
    qboolean wind_affected;   // Affected by wind
    float wind_strength;      // Wind effect strength
    qboolean fade_with_distance; // Distance-based fading
    float max_distance;       // Maximum render distance
    float density;            // Sprites per unit area
} surface_sprite_type_t;

// Individual surface sprite instance
typedef struct {
    vec3_t position;          // World position
    vec3_t normal;            // Surface normal
    vec3_t tangent;           // Surface tangent
    float scale;              // Size scale
    float rotation;           // Rotation angle
    float animation_time;     // Animation timer
    int current_frame;        // Current animation frame
    vec3_t color_offset;      // Color variation
    float alpha_offset;       // Alpha variation
} surface_sprite_t;

// Surface sprite batch for rendering
typedef struct {
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    int sprite_count;
    int vertex_count;
    int index_count;
    qboolean dirty;           // Needs re-upload
} surface_sprite_batch_t;

// Surface sprites system state
typedef struct {
    qboolean initialized;
    qboolean enabled;

    // Sprite types
    surface_sprite_type_t types[MAX_SURFACE_SPRITE_TYPES];
    int num_types;

    // Sprite instances
    surface_sprite_t *sprites;
    int num_sprites;
    int max_sprites;

    // Rendering batches
    surface_sprite_batch_t *batches;
    int num_batches;
    int max_batches;

    // Vulkan resources
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    // Wind system
    vec3_t wind_direction;
    float wind_strength;
    float wind_frequency;

    // Performance stats
    int visible_sprites;
    int culled_sprites;

} surface_sprites_system_t;

// External API
void vk_surface_sprites_init(void);
void vk_surface_sprites_shutdown(void);
void vk_surface_sprites_update(void);
void vk_surface_sprites_render(void);

// Sprite type management
int vk_surface_sprites_register_type(const char *name, const char *texture_path,
                                   vec2_t size, vec3_t color, float alpha,
                                   qboolean animated, int num_frames, float frame_time);

void vk_surface_sprites_unregister_type(int type_index);

// Sprite instance management
int vk_surface_sprites_add_sprite(int type_index, const vec3_t position, const vec3_t normal,
                                float scale, float rotation);

void vk_surface_sprites_remove_sprite(int sprite_index);
void vk_surface_sprites_clear_all(void);

// Terrain integration
void vk_surface_sprites_populate_terrain(int type_index, const vec3_t mins, const vec3_t maxs,
                                       float density, qboolean use_heightmap);

// Wind system
void vk_surface_sprites_set_wind(const vec3_t direction, float strength, float frequency);
void vk_surface_sprites_get_wind(vec3_t direction, float *strength, float *frequency);

// Utility functions
int vk_surface_sprites_get_type_count(void);
int vk_surface_sprites_get_sprite_count(void);
surface_sprite_type_t *vk_surface_sprites_get_type(int index);
qboolean vk_surface_sprites_trace(const vec3_t start, const vec3_t end, vec3_t hit_pos, int *sprite_index);

// CVars
extern cvar_t *r_surfaceSprites;
extern cvar_t *r_surfaceSpritesMax;
extern cvar_t *r_surfaceSpritesDistance;
extern cvar_t *r_surfaceSpritesWind;

#endif // USE_VULKAN