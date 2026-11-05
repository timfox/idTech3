#pragma once


/*
 * Raster Ultra 1.11 — Rendering Reference Lab.
 * Deterministic validation / capture / decomposition ownership.
 * Does NOT add new rendering techniques.
 */

typedef enum {
	VK_REFLAB_SCENE_MATERIAL_SPHERES = 0,
	VK_REFLAB_SCENE_ROUGH_METAL_SWEEP,
	VK_REFLAB_SCENE_ADVANCED_LOBES,
	VK_REFLAB_SCENE_HARD_EDGES,
	VK_REFLAB_SCENE_TANGENT_PARITY,
	VK_REFLAB_SCENE_DIRECT_LIGHTS,
	VK_REFLAB_SCENE_AREA_LIGHTS,
	VK_REFLAB_SCENE_SHADOWS,
	VK_REFLAB_SCENE_GI,
	VK_REFLAB_SCENE_REFLECTIONS,
	VK_REFLAB_SCENE_WATER,
	VK_REFLAB_SCENE_TRANSPARENCY,
	VK_REFLAB_SCENE_PARTICLES,
	VK_REFLAB_SCENE_DECALS,
	VK_REFLAB_SCENE_VOLUMETRICS,
	VK_REFLAB_SCENE_ATMOSPHERE,
	VK_REFLAB_SCENE_WEATHER,
	VK_REFLAB_SCENE_TERRAIN,
	VK_REFLAB_SCENE_FOLIAGE,
	VK_REFLAB_SCENE_LOD,
	VK_REFLAB_SCENE_STREAMING,
	VK_REFLAB_SCENE_HDR_PRESENTATION,
	VK_REFLAB_SCENE_WEAPON_UI,
	VK_REFLAB_SCENE_COUNT
} vkRefLabScene_t;

typedef enum {
	VK_REFLAB_DECOMP_FINAL = 0,
	VK_REFLAB_DECOMP_DIRECT,
	VK_REFLAB_DECOMP_INDIRECT,
	VK_REFLAB_DECOMP_SHADOWS,
	VK_REFLAB_DECOMP_AO,
	VK_REFLAB_DECOMP_REFLECTIONS,
	VK_REFLAB_DECOMP_EMISSIVE,
	VK_REFLAB_DECOMP_VOLUMETRICS,
	VK_REFLAB_DECOMP_COUNT
} vkRefLabDecomp_t;

typedef enum {
	VK_REFLAB_REF_NONE = 0,
	VK_REFLAB_REF_SPATIAL_2X,
	VK_REFLAB_REF_SPATIAL_4X,
	VK_REFLAB_REF_SPATIAL_8X,
	VK_REFLAB_REF_MATERIAL,
	VK_REFLAB_REF_LIGHTING,
	VK_REFLAB_REF_PRESENTATION
} vkRefLabReferenceMode_t;

typedef struct vkRefLabBookmark_s {
	const char *name;
	float origin[3];
	float angles[3]; /* pitch yaw roll */
} vkRefLabBookmark_t;

typedef struct vkRefLabState_s {
	vkRefLabScene_t scene;
	vkRefLabDecomp_t decomp;
	vkRefLabReferenceMode_t referenceMode;
	uint32_t seed;
	uint32_t frameCount;
	qboolean deterministic;
	qboolean freezeAnimation;
	qboolean freezeWeather;
	qboolean freezeExposure;
	qboolean freezeTemporalJitter;
	qboolean noBloom;
	qboolean noGrain;
	qboolean noLens;
	qboolean supersampleActive;
	int supersampleScale; /* 1,2,4,8 */
	const char *mapHint;
	const char *cfgHint;
} vkRefLabState_t;

void vk_reference_lab_register_cvars( void );
void vk_reference_lab_init( void );
void vk_reference_lab_shutdown( void );

qboolean vk_reference_lab_active( void );
const vkRefLabState_t *vk_reference_lab_state( void );

const char *vk_reference_lab_scene_name( vkRefLabScene_t scene );
const char *vk_reference_lab_decomp_name( vkRefLabDecomp_t d );
int vk_reference_lab_bookmark_count( vkRefLabScene_t scene );
const vkRefLabBookmark_t *vk_reference_lab_bookmark( vkRefLabScene_t scene, int index );

/* Apply deterministic pins each frame while lab is active. */
void vk_reference_lab_begin_frame( void );

void vk_reference_lab_status_f( void );
void vk_reference_lab_list_scenes_f( void );

