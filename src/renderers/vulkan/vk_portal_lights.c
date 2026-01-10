/*
=============================================================================
Portal Lights System Implementation
Converts sky clusters to analytic area lights for ray tracing
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk_portal_lights.h"

#ifdef USE_VULKAN_RAY_TRACING

#include <string.h>
#include <stdlib.h>

static portalLightSystem_t portalSystem;

// Forward declarations for local functions
qboolean vk_portal_lights_create_buffer(void);
void vk_portal_lights_destroy_buffer(void);
void vk_portal_lights_update_buffer(void);

// Parse sky cluster file for a given map
static void parse_sky_clusters(const char* mapName) {
	char filename[MAX_QPATH];
	char* buffer;
	int len;

	// Reset sky clusters
	portalSystem.numSkyClusters = 0;

	// Try to load sky cluster file
	Com_sprintf(filename, sizeof(filename), "maps/%s.txt", mapName);

	len = ri.FS_ReadFile(filename, (void**)&buffer);
	if (len < 0 || !buffer) {
		ri.Printf(PRINT_DEVELOPER, "No sky clusters file found for map %s\n", mapName);
		return;
	}

	char* ptr = buffer;
	char line[1024];
	int lineNum = 0;

	while (*ptr && portalSystem.numSkyClusters < MAX_SKY_CLUSTERS) {
		// Read line
		char* lineEnd = strchr(ptr, '\n');
		if (!lineEnd) {
			// Last line
			Q_strncpyz(line, ptr, sizeof(line));
			ptr += strlen(ptr);
		} else {
			int lineLen = lineEnd - ptr;
			if (lineLen >= sizeof(line)) lineLen = sizeof(line) - 1;
			memcpy(line, ptr, lineLen);
			line[lineLen] = 0;
			ptr = lineEnd + 1;
		}

		lineNum++;

		// Trim whitespace and skip comments/empty lines
		char* trimmed = line;
		while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

		if (!*trimmed || *trimmed == '#' || *trimmed == '/' || *trimmed == ';') {
			continue;
		}

		// Parse cluster IDs from this line
		char* token = strtok(trimmed, " \t");
		while (token && portalSystem.numSkyClusters < MAX_SKY_CLUSTERS) {
			int clusterId = atoi(token);
			if (clusterId >= 0 && clusterId < ri.CM_NumClusters()) {
				skyCluster_t* cluster = &portalSystem.skyClusters[portalSystem.numSkyClusters++];
				cluster->clusterId = clusterId;
				cluster->isSky = qtrue;  // Assume sky for now (could be extended for lava)
				cluster->processed = qfalse;

				ri.Printf(PRINT_DEVELOPER, "Added sky cluster %d from line %d\n", clusterId, lineNum);
			}
			token = strtok(NULL, " \t");
		}
	}

	ri.FS_FreeFile(buffer);

	ri.Printf(PRINT_ALL, "Loaded %d sky clusters for map %s\n", portalSystem.numSkyClusters, mapName);
}

// Generate portal lights from sky clusters
static void generate_portal_lights(void) {
	portalSystem.numLights = 0;

	// For each sky cluster, find surfaces that face the sky
	for (int i = 0; i < portalSystem.numSkyClusters && portalSystem.numLights < MAX_PORTAL_LIGHTS; i++) {
		skyCluster_t* cluster = &portalSystem.skyClusters[i];

		if (cluster->processed) continue;

		// TODO: This is a simplified implementation
		// In a full implementation, we would:
		// 1. Iterate through all BSP surfaces in this cluster
		// 2. Find surfaces marked as SKY or facing sky openings
		// 3. Calculate light position, normal, area, and color
		// 4. Create portal light entries

		// For now, create a placeholder light for each sky cluster
		portalLight_t* light = &portalSystem.lights[portalSystem.numLights++];

		// Calculate approximate position (this is simplified)
		// In reality, we'd compute centroid of sky-facing surfaces
		VectorSet(light->position, 0, 0, 0);  // Would be computed from BSP geometry
		VectorSet(light->normal, 0, 0, 1);    // Up vector for sky lights
		VectorSet(light->color, 1.0f, 1.0f, 1.0f);  // White sky light
		light->radius = 1000.0f;  // Large radius for sky lights
		light->area = 10000.0f;   // Large area
		light->cluster = cluster->clusterId;
		light->active = qtrue;

		cluster->processed = qtrue;

		ri.Printf(PRINT_DEVELOPER, "Generated portal light for cluster %d\n", cluster->clusterId);
	}

	ri.Printf(PRINT_ALL, "Generated %d portal lights from sky clusters\n", portalSystem.numLights);
}

// Update portal light colors based on current sky color
static void update_portal_light_colors(void) {
	// Sample the current skybox and update portal light colors to match
	// This makes portal lights match the sky color for realistic lighting
	
	extern trGlobals_t tr;
	
	// Get sky color from renderer
	// The sky color can be derived from the current shader's sky configuration
	// or sampled from environment lighting
	vec3_t skyColor = {1.0f, 1.0f, 1.0f}; // Default white
	
	// Try to get sky color from current shader or environment
	// In idTech3, sky information is typically in the shader system
	// For now, use a default sky color based on typical outdoor lighting
	// In a full implementation, this would sample the skybox cubemap or
	// query the current sky shader's color
	
	// Use a neutral sky color that matches typical outdoor lighting
	// This provides a reasonable approximation when skybox sampling isn't available
	skyColor[0] = 0.6f; // Slightly blue-tinted sky
	skyColor[1] = 0.7f;
	skyColor[2] = 0.9f;
	
	// Note: Full implementation would:
	// 1. Query current sky shader from tr.shaders
	// 2. Sample skybox cubemap texture if available
	// 3. Use time-of-day or environment settings to adjust color
	
	// Apply sky color to all portal lights
	for (int i = 0; i < portalSystem.numLights; i++) {
		portalLight_t* light = &portalSystem.lights[i];
		
		// Blend sky color with existing light color
		// This allows lights to maintain some of their original color
		// while being influenced by the sky
		vec3_t blendedColor;
		VectorScale(skyColor, 0.7f, blendedColor);
		vec3_t lightColor = {light->color[0], light->color[1], light->color[2]};
		VectorScale(lightColor, 0.3f, lightColor);
		VectorAdd(blendedColor, lightColor, blendedColor);
		
		// Clamp to valid range
		blendedColor[0] = (blendedColor[0] < 0.0f) ? 0.0f : ((blendedColor[0] > 1.0f) ? 1.0f : blendedColor[0]);
		blendedColor[1] = (blendedColor[1] < 0.0f) ? 0.0f : ((blendedColor[1] > 1.0f) ? 1.0f : blendedColor[1]);
		blendedColor[2] = (blendedColor[2] < 0.0f) ? 0.0f : ((blendedColor[2] > 1.0f) ? 1.0f : blendedColor[2]);
		
		VectorCopy(blendedColor, light->color);
	}
}

void vk_portal_lights_init(void) {
	memset(&portalSystem, 0, sizeof(portalSystem));
	portalSystem.initialized = qtrue;
	portalSystem.enabled = qtrue;  // Could be controlled by CVAR

	// Create GPU buffer for portal lights
	if (!vk_portal_lights_create_buffer()) {
		ri.Printf(PRINT_WARNING, "Failed to create portal lights buffer\n");
		portalSystem.enabled = qfalse;
	}

	ri.Printf(PRINT_DEVELOPER, "Portal light system initialized\n");
}

void vk_portal_lights_shutdown(void) {
	// Destroy GPU buffer
	vk_portal_lights_destroy_buffer();

	memset(&portalSystem, 0, sizeof(portalSystem));
	portalSystem.initialized = qfalse;

	ri.Printf(PRINT_DEVELOPER, "Portal light system shutdown\n");
}

void vk_portal_lights_load_map(const char* mapName) {
	if (!portalSystem.initialized || !mapName) return;

	Q_strncpyz(portalSystem.currentMapName, mapName, sizeof(portalSystem.currentMapName));

	// Load sky clusters for this map
	parse_sky_clusters(mapName);

	// Generate portal lights
	generate_portal_lights();

	ri.Printf(PRINT_ALL, "Portal lights loaded for map %s\n", mapName);
}

void vk_portal_lights_update_lights(void) {
	if (!portalSystem.initialized || !portalSystem.enabled) return;

	update_portal_light_colors();

	// Update GPU buffer with current light data
	vk_portal_lights_update_buffer();
}

int vk_portal_lights_get_count(void) {
	return portalSystem.numLights;
}

const portalLight_t* vk_portal_lights_get_light(int index) {
	if (index < 0 || index >= portalSystem.numLights) return NULL;
	return &portalSystem.lights[index];
}

qboolean vk_portal_lights_cluster_is_sky(int clusterId) {
	for (int i = 0; i < portalSystem.numSkyClusters; i++) {
		if (portalSystem.skyClusters[i].clusterId == clusterId) {
			return portalSystem.skyClusters[i].isSky;
		}
	}
	return qfalse;
}

// Create GPU buffer for portal lights
qboolean vk_portal_lights_create_buffer(void) {
	if (!vk.device || vk.portalLightsBuffer != VK_NULL_HANDLE) {
		return qtrue; // Already created or no device
	}

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = MAX_PORTAL_LIGHTS * sizeof(portalLight_t);
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(qvkCreateBuffer(vk.device, &bufferInfo, NULL, &vk.portalLightsBuffer));

	// Allocate memory
	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements(vk.device, vk.portalLightsBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk.portalLightsBufferMemory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.portalLightsBuffer, vk.portalLightsBufferMemory, 0));

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.portalLightsDescriptorSetLayout));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocSetInfo = {};
	allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocSetInfo.descriptorPool = vk.descriptor_pool;
	allocSetInfo.descriptorSetCount = 1;
	allocSetInfo.pSetLayouts = &vk.portalLightsDescriptorSetLayout;

	VK_CHECK(qvkAllocateDescriptorSets(vk.device, &allocSetInfo, &vk.portalLightsDescriptorSet));

	// Update descriptor set
	VkDescriptorBufferInfo bufferInfoDesc = {};
	bufferInfoDesc.buffer = vk.portalLightsBuffer;
	bufferInfoDesc.offset = 0;
	bufferInfoDesc.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writeSet = {};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = vk.portalLightsDescriptorSet;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeSet.pBufferInfo = &bufferInfoDesc;

	qvkUpdateDescriptorSets(vk.device, 1, &writeSet, 0, NULL);

	SET_OBJECT_NAME(vk.portalLightsBuffer, "portal lights buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(vk.portalLightsBufferMemory, "portal lights buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);

	ri.Printf(PRINT_DEVELOPER, "Created portal lights buffer with capacity for %d lights\n", MAX_PORTAL_LIGHTS);
	return qtrue;
}

// Update GPU buffer with current portal light data
void vk_portal_lights_update_buffer(void) {
	if (!vk.device || vk.portalLightsBuffer == VK_NULL_HANDLE) {
		return;
	}

	// Map buffer and upload data
	void* mappedData;
	VK_CHECK(qvkMapMemory(vk.device, vk.portalLightsBufferMemory, 0, VK_WHOLE_SIZE, 0, &mappedData));

	// Copy portal lights data
	portalLight_t* bufferLights = (portalLight_t*)mappedData;
	for (int i = 0; i < MAX_PORTAL_LIGHTS; i++) {
		if (i < portalSystem.numLights) {
			bufferLights[i] = portalSystem.lights[i];
		} else {
			// Clear unused slots
			memset(&bufferLights[i], 0, sizeof(portalLight_t));
			bufferLights[i].active = qfalse;
		}
	}

	qvkUnmapMemory(vk.device, vk.portalLightsBufferMemory);

	ri.Printf(PRINT_DEVELOPER, "Updated portal lights buffer with %d lights\n", portalSystem.numLights);
}

// Destroy portal lights buffer
void vk_portal_lights_destroy_buffer(void) {
	if (vk.portalLightsDescriptorSet != VK_NULL_HANDLE) {
		qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &vk.portalLightsDescriptorSet);
		vk.portalLightsDescriptorSet = VK_NULL_HANDLE;
	}

	if (vk.portalLightsDescriptorSetLayout != VK_NULL_HANDLE) {
		qvkDestroyDescriptorSetLayout(vk.device, vk.portalLightsDescriptorSetLayout, NULL);
		vk.portalLightsDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (vk.portalLightsBuffer != VK_NULL_HANDLE) {
		qvkDestroyBuffer(vk.device, vk.portalLightsBuffer, NULL);
		vk.portalLightsBuffer = VK_NULL_HANDLE;
	}

	if (vk.portalLightsBufferMemory != VK_NULL_HANDLE) {
		qvkFreeMemory(vk.device, vk.portalLightsBufferMemory, NULL);
		vk.portalLightsBufferMemory = VK_NULL_HANDLE;
	}

	ri.Printf(PRINT_DEVELOPER, "Destroyed portal lights buffer\n");
}

void vk_portal_lights_debug_draw(void) {
	// Debug visualization of portal lights
	// This would draw wireframe spheres or arrows at light positions
	// For now, just print debug info

	if (!portalSystem.initialized) return;

	ri.Printf(PRINT_ALL, "Portal Lights Debug:\n");
	ri.Printf(PRINT_ALL, "  Total lights: %d\n", portalSystem.numLights);
	ri.Printf(PRINT_ALL, "  Sky clusters: %d\n", portalSystem.numSkyClusters);

	for (int i = 0; i < portalSystem.numLights; i++) {
		const portalLight_t* light = &portalSystem.lights[i];
		ri.Printf(PRINT_ALL, "  Light %d: pos(%.1f,%.1f,%.1f) cluster=%d active=%d\n",
				 i, light->position[0], light->position[1], light->position[2],
				 light->cluster, light->active);
	}
}

#endif // USE_VULKAN_RAY_TRACING
