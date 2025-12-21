/*
=============================================================================
Portal Lights System for Ray Tracing
Converts sky clusters to analytic area lights for better GI quality
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN_RAY_TRACING

#define MAX_PORTAL_LIGHTS 256
#define MAX_SKY_CLUSTERS 1024

// Portal light structure
typedef struct {
	vec3_t position;        // Light position
	vec3_t normal;          // Light normal (direction)
	vec3_t color;           // Light color/intensity
	float radius;           // Light radius
	float area;             // Light surface area
	int cluster;            // BSP cluster this light represents
	qboolean active;        // Whether this light is active
} portalLight_t;

// Sky cluster definition
typedef struct {
	int clusterId;          // BSP cluster ID
	qboolean isSky;         // True for sky clusters, false for lava
	qboolean processed;     // Whether this cluster has been processed
} skyCluster_t;

// Portal light system state
typedef struct {
	portalLight_t lights[MAX_PORTAL_LIGHTS];
	int numLights;

	skyCluster_t skyClusters[MAX_SKY_CLUSTERS];
	int numSkyClusters;

	char currentMapName[MAX_QPATH];  // Current map name for cluster loading

	qboolean initialized;
	qboolean enabled;
} portalLightSystem_t;

// Portal light API
void vk_portal_lights_init(void);
void vk_portal_lights_shutdown(void);
void vk_portal_lights_load_map(const char* mapName);
void vk_portal_lights_update_lights(void);

// Ray tracing integration
int vk_portal_lights_get_count(void);
const portalLight_t* vk_portal_lights_get_light(int index);
qboolean vk_portal_lights_cluster_is_sky(int clusterId);

// Debug visualization
void vk_portal_lights_debug_draw(void);

#endif // USE_VULKAN_RAY_TRACING
