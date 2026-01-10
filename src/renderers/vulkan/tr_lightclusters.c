/*
===========================================================================
Light Clustering (Vulkan renderer)

CPU-side light clustering/tiling for clustered forward+ rendering.
Bins dynamic lights into screen-space tiles and depth slices,
enabling efficient per-pixel light iteration in shaders.

Implementation status: Complete - full implementation with Vulkan buffers
===========================================================================
*/

#include "tr_local.h"
#include "../renderercommon/tr_lightclusters.h"
#include "vk.h"
#include "vk_memory.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;

// Forward declarations
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern VkCommandBuffer begin_command_buffer(void);
extern void vk_flush_staging_buffer(qboolean final);
extern const char *vk_result_string(VkResult result);
extern Vk_Instance vk;
extern PFN_vkCmdCopyBuffer qvkCmdCopyBuffer;

// Configuration: tile size and Z slices (matches OpenGL implementation)
static const int lc_tileSize = 16; // pixels per tile
static const int lc_slicesZ = 16;  // depth slices

// Vulkan buffer storage for cluster headers and indices
static VkBuffer lcHeaderBuffer = VK_NULL_HANDLE;
static VkBuffer lcIndexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory lcHeaderMemory = VK_NULL_HANDLE;
static VkDeviceMemory lcIndexMemory = VK_NULL_HANDLE;
static VkDeviceSize lcHeaderBufferSize = 0;
static VkDeviceSize lcIndexBufferSize = 0;
static qboolean lc_buffers_initialized = qfalse;

// CPU-side temporary buffers for binning
static lc_cluster_header_t lc_headers[LC_MAX_CLUSTERS];
static int lc_indices[LC_MAX_CLUSTERS * LC_MAX_LIGHTS_PER_CLUSTER];

// Compute grid dimensions for current viewport
static lc_grid_params_t LC_ComputeGrid(void) {
	lc_grid_params_t g = {0};
	const int width = glConfig.vidWidth;
	const int height = glConfig.vidHeight;
	g.tilesX = (width  + lc_tileSize - 1) / lc_tileSize;
	g.tilesY = (height + lc_tileSize - 1) / lc_tileSize;
	g.slicesZ = lc_slicesZ;
	g.zNear = r_znear->value;
	g.zFar = tr.viewParms.zFar;
	// Logarithmic depth slicing
	g.invLogZ = 1.0f / logf(g.zFar / g.zNear);
	
	// Clamp to safety cap
	if (g.tilesX * g.tilesY * g.slicesZ > LC_MAX_CLUSTERS) {
		const float scale = sqrtf((float)LC_MAX_CLUSTERS / (float)(g.tilesX * g.tilesY * g.slicesZ));
		g.tilesX = (int)(g.tilesX * scale);
		g.tilesY = (int)(g.tilesY * scale);
		if (g.tilesX < 1) g.tilesX = 1;
		if (g.tilesY < 1) g.tilesY = 1;
	}
	return g;
}

// Map depth to slice using logarithmic distribution
static int LC_DepthToSlice(const lc_grid_params_t *g, float viewZ) {
	const float z = -viewZ; // view space forward is -Z
	if (z <= g->zNear) return 0;
	if (z >= g->zFar) return g->slicesZ - 1;
	const float n = logf(z / g->zNear) * g->invLogZ; // 0..1
	int slice = (int)(n * g->slicesZ);
	if (slice < 0) slice = 0;
	if (slice >= g->slicesZ) slice = g->slicesZ - 1;
	return slice;
}

// Project a point to screen; returns false if behind the near plane
static qboolean LC_ProjectToScreen(const vec3_t viewPos, int *outX, int *outY) {
	// Simple perspective projection using current projection matrix
	const float *m = tr.viewParms.projectionMatrix;
	const float x = viewPos[0], y = viewPos[1], z = viewPos[2];
	const float clipX = m[0]*x + m[4]*y + m[8]*z + m[12];
	const float clipY = m[1]*x + m[5]*y + m[9]*z + m[13];
	const float clipW = m[3]*x + m[7]*y + m[11]*z + m[15];
	if (clipW >= 0.0f) {
		return qfalse; // behind eye
	}
	const float invW = 1.0f / clipW;
	const float ndcX = clipX * invW;
	const float ndcY = clipY * invW;
	*outX = (int)((ndcX * 0.5f + 0.5f) * glConfig.vidWidth);
	*outY = (int)((ndcY * 0.5f + 0.5f) * glConfig.vidHeight);
	return qtrue;
}

// Initialize or resize Vulkan buffers for light clustering
static void LC_InitBuffers(VkDeviceSize headerSize, VkDeviceSize indexSize) {
	VkBufferCreateInfo bufferInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkResult result;

	// Clean up existing buffers if resizing
	if (lc_buffers_initialized && (headerSize > lcHeaderBufferSize || indexSize > lcIndexBufferSize)) {
		if (lcHeaderBuffer != VK_NULL_HANDLE) {
			qvkDestroyBuffer(vk.device, lcHeaderBuffer, NULL);
			qvkFreeMemory(vk.device, lcHeaderMemory, NULL);
			lcHeaderBuffer = VK_NULL_HANDLE;
			lcHeaderMemory = VK_NULL_HANDLE;
		}
		if (lcIndexBuffer != VK_NULL_HANDLE) {
			qvkDestroyBuffer(vk.device, lcIndexBuffer, NULL);
			qvkFreeMemory(vk.device, lcIndexMemory, NULL);
			lcIndexBuffer = VK_NULL_HANDLE;
			lcIndexMemory = VK_NULL_HANDLE;
		}
		lc_buffers_initialized = qfalse;
	}

	if (lc_buffers_initialized) {
		return; // Buffers already exist and are large enough
	}

	// Create header buffer
	Com_Memset(&bufferInfo, 0, sizeof(bufferInfo));
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = headerSize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = qvkCreateBuffer(vk.device, &bufferInfo, NULL, &lcHeaderBuffer);
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "LC_InitBuffers: Failed to create header buffer: %s\n", vk_result_string(result));
		return;
	}

	qvkGetBufferMemoryRequirements(vk.device, lcHeaderBuffer, &memReqs);
	Com_Memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &lcHeaderMemory);
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "LC_InitBuffers: Failed to allocate header memory: %s\n", vk_result_string(result));
		qvkDestroyBuffer(vk.device, lcHeaderBuffer, NULL);
		return;
	}

	qvkBindBufferMemory(vk.device, lcHeaderBuffer, lcHeaderMemory, 0);
	lcHeaderBufferSize = headerSize;
	SET_OBJECT_NAME(lcHeaderBuffer, "light_cluster_headers", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

	// Create index buffer
	Com_Memset(&bufferInfo, 0, sizeof(bufferInfo));
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = indexSize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = qvkCreateBuffer(vk.device, &bufferInfo, NULL, &lcIndexBuffer);
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "LC_InitBuffers: Failed to create index buffer: %s\n", vk_result_string(result));
		qvkFreeMemory(vk.device, lcHeaderMemory, NULL);
		qvkDestroyBuffer(vk.device, lcHeaderBuffer, NULL);
		return;
	}

	qvkGetBufferMemoryRequirements(vk.device, lcIndexBuffer, &memReqs);
	Com_Memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &lcIndexMemory);
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_ERROR, "LC_InitBuffers: Failed to allocate index memory: %s\n", vk_result_string(result));
		qvkDestroyBuffer(vk.device, lcIndexBuffer, NULL);
		qvkFreeMemory(vk.device, lcHeaderMemory, NULL);
		qvkDestroyBuffer(vk.device, lcHeaderBuffer, NULL);
		return;
	}

	qvkBindBufferMemory(vk.device, lcIndexBuffer, lcIndexMemory, 0);
	lcIndexBufferSize = indexSize;
	lc_buffers_initialized = qtrue;
	SET_OBJECT_NAME(lcIndexBuffer, "light_cluster_indices", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

	ri.Printf(PRINT_DEVELOPER, "LC_InitBuffers: Created buffers (header: %lu, index: %lu bytes)\n",
		(unsigned long)headerSize, (unsigned long)indexSize);
}

// Upload cluster data to GPU using staging buffer
static void LC_UploadBuffers(VkDeviceSize headerSize, VkDeviceSize indexSize) {
	VkCommandBuffer cmd = begin_command_buffer();
	VkBufferCopy copyRegion;

	if (!cmd || !lc_buffers_initialized) {
		return;
	}

	// Check staging buffer size
	if (headerSize + indexSize > vk.staging_buffer.size) {
		ri.Printf(PRINT_WARNING, "LC_UploadBuffers: Data too large for staging buffer\n");
		return;
	}

	// Copy header data to staging buffer
	Com_Memcpy(vk.staging_buffer.ptr, lc_headers, (size_t)headerSize);

	// Copy header buffer
	Com_Memset(&copyRegion, 0, sizeof(copyRegion));
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = headerSize;
	qvkCmdCopyBuffer(cmd, vk.staging_buffer.handle, lcHeaderBuffer, 1, &copyRegion);

	// Copy index data to staging buffer
	Com_Memcpy((byte*)vk.staging_buffer.ptr + headerSize, lc_indices, (size_t)indexSize);

	// Copy index buffer
	copyRegion.srcOffset = headerSize;
	copyRegion.dstOffset = 0;
	copyRegion.size = indexSize;
	qvkCmdCopyBuffer(cmd, vk.staging_buffer.handle, lcIndexBuffer, 1, &copyRegion);

	vk_flush_staging_buffer(qfalse);
}

void R_BuildLightClusters( void ) {
	// Early return if not registered or no lights
	if (!tr.registered || tr.refdef.num_dlights <= 0) {
		return;
	}
	
	// Check if clustering is enabled
	extern cvar_t *r_clusteredLight;
	if (!r_clusteredLight || !r_clusteredLight->integer) {
		return;
	}

	// Compute grid parameters
	const lc_grid_params_t grid = LC_ComputeGrid();
	const int clusterCount = grid.tilesX * grid.tilesY * grid.slicesZ;
	if (clusterCount <= 0 || clusterCount > LC_MAX_CLUSTERS) {
		return;
	}

	// Calculate buffer sizes
	const VkDeviceSize headerSize = clusterCount * sizeof(lc_cluster_header_t);
	const VkDeviceSize indexSize = clusterCount * LC_MAX_LIGHTS_PER_CLUSTER * sizeof(int);

	// Initialize/resize buffers if needed
	LC_InitBuffers(headerSize, indexSize);
	if (!lc_buffers_initialized) {
		return;
	}

	// Reset cluster headers
	for (int i = 0; i < clusterCount; ++i) {
		lc_headers[i].lightOffset = i * LC_MAX_LIGHTS_PER_CLUSTER;
		lc_headers[i].lightCount = 0;
	}

	// For each light, determine affected cluster bounds and append
	const int numLights = (int)tr.refdef.num_dlights;
	for (int li = 0; li < numLights && li < LC_MAX_LIGHTS; ++li) {
		const dlight_t *dl = &tr.refdef.dlights[li];

		// Transform to view space
		vec3_t vpos;
		const orientationr_t *viewOr = &tr.viewParms.or;
		for (int a = 0; a < 3; ++a) {
			vpos[a] = DotProduct(dl->origin, viewOr->axis[a]) - viewOr->origin[a];
		}

		// Depth slice range
		const float radius = dl->radius;
		const float zMin = vpos[2] - radius;
		const float zMax = vpos[2] + radius;
		int sliceMin = LC_DepthToSlice(&grid, zMax);
		int sliceMax = LC_DepthToSlice(&grid, zMin);
		if (sliceMax < 0 || sliceMin >= grid.slicesZ) {
			continue; // out of range
		}
		if (sliceMin < 0) sliceMin = 0;
		if (sliceMax >= grid.slicesZ) sliceMax = grid.slicesZ - 1;

		// Screen-space bounds: project center and approximate radius in screen space
		int sx = 0, sy = 0;
		if (!LC_ProjectToScreen(vpos, &sx, &sy)) {
			continue;
		}
		if (vpos[2] >= -grid.zNear) {
			continue; // behind or too close
		}

		// Approximate screen radius (pixels) using projection matrix scale
		const float invZ = -1.0f / vpos[2];
		const float projScaleX = tr.viewParms.projectionMatrix[0];
		const float projScaleY = tr.viewParms.projectionMatrix[5];
		const float srX = radius * projScaleX * invZ * 0.5f * glConfig.vidWidth;
		const float srY = radius * projScaleY * invZ * 0.5f * glConfig.vidHeight;

		const int minX = Com_Clamp(0, glConfig.vidWidth  - 1, (int)floorf(sx - srX));
		const int maxX = Com_Clamp(0, glConfig.vidWidth  - 1, (int)ceilf (sx + srX));
		const int minY = Com_Clamp(0, glConfig.vidHeight - 1, (int)floorf(sy - srY));
		const int maxY = Com_Clamp(0, glConfig.vidHeight - 1, (int)ceilf (sy + srY));

		const int tileMinX = Com_Clamp(0, grid.tilesX - 1, minX / lc_tileSize);
		const int tileMaxX = Com_Clamp(0, grid.tilesX - 1, maxX / lc_tileSize);
		const int tileMinY = Com_Clamp(0, grid.tilesY - 1, minY / lc_tileSize);
		const int tileMaxY = Com_Clamp(0, grid.tilesY - 1, maxY / lc_tileSize);

		// Append light index to affected clusters
		for (int z = sliceMin; z <= sliceMax; ++z) {
			for (int ty = tileMinY; ty <= tileMaxY; ++ty) {
				for (int tx = tileMinX; tx <= tileMaxX; ++tx) {
					const int clusterIndex = (z * grid.tilesY + ty) * grid.tilesX + tx;
					lc_cluster_header_t *hdr = &lc_headers[clusterIndex];
					if (hdr->lightCount < LC_MAX_LIGHTS_PER_CLUSTER) {
						lc_indices[hdr->lightOffset + hdr->lightCount] = li;
						++hdr->lightCount;
					}
				}
			}
		}
	}

	// Upload cluster data to GPU buffers
	LC_UploadBuffers(headerSize, indexSize);

	// Note: Descriptor set binding should be handled by the shader system
	// when setting up lighting descriptor sets. The buffers are now ready
	// to be bound at bindings 6 (headers) and 7 (indices) per tr_lightclusters.glsl
}

// Cleanup function for light clustering buffers
void R_ShutdownLightClusters(void) {
	if (lcHeaderBuffer != VK_NULL_HANDLE) {
		qvkDestroyBuffer(vk.device, lcHeaderBuffer, NULL);
		lcHeaderBuffer = VK_NULL_HANDLE;
	}
	if (lcHeaderMemory != VK_NULL_HANDLE) {
		qvkFreeMemory(vk.device, lcHeaderMemory, NULL);
		lcHeaderMemory = VK_NULL_HANDLE;
	}
	if (lcIndexBuffer != VK_NULL_HANDLE) {
		qvkDestroyBuffer(vk.device, lcIndexBuffer, NULL);
		lcIndexBuffer = VK_NULL_HANDLE;
	}
	if (lcIndexMemory != VK_NULL_HANDLE) {
		qvkFreeMemory(vk.device, lcIndexMemory, NULL);
		lcIndexMemory = VK_NULL_HANDLE;
	}
	lc_buffers_initialized = qfalse;
	lcHeaderBufferSize = 0;
	lcIndexBufferSize = 0;
}


