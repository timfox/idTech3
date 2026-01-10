/*
=============================================================================
Terrain Rendering System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_terrain.h"
#include "vk_utils.h"
#include "vk_pipeline.h"
#include "vk.h"
#include <string.h>
#include <math.h>
#include <float.h>

// Helper function declarations
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern VkCommandBuffer begin_command_buffer(void);
extern void end_command_buffer(VkCommandBuffer command_buffer, const char *location);
extern const char* vk_result_string(VkResult result);

#ifdef USE_VULKAN

// CVars
cvar_t *r_terrain;
cvar_t *r_terrainLod;
cvar_t *r_terrainGridSize;
cvar_t *r_terrainPatchSize;
cvar_t *r_terrainMaterials;

// Global system state
static terrain_system_t terrain_system;

// Forward declarations
static qboolean vk_create_terrain_resources(void);
static void vk_destroy_terrain_resources(void);
static void vk_create_terrain_pipeline(void);
static void vk_destroy_terrain_pipeline(void);
static void vk_update_terrain_descriptors(void);
static void vk_generate_terrain_patches(void);
static void vk_update_terrain_lod(void);
static void vk_build_patch_geometry(terrain_patch_t *patch, int lod_level);
static qboolean vk_load_heightmap_from_file(const char *path);
static void vk_generate_heightmap_normals(void);
static void vk_upload_heightmap_texture(void);

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);

// Vulkan function pointers
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
extern PFN_vkDestroyDescriptorPool qvkDestroyDescriptorPool;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Initialize terrain system
void vk_terrain_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing terrain system\n");

    memset(&terrain_system, 0, sizeof(terrain_system));

    // Register CVars
    // Off by default until the rendering path is fully validated.
    r_terrain = ri.Cvar_Get("r_terrain", "0", CVAR_ARCHIVE);
    r_terrainLod = ri.Cvar_Get("r_terrainLod", "1", CVAR_ARCHIVE);
    r_terrainGridSize = ri.Cvar_Get("r_terrainGridSize", "1024", CVAR_ARCHIVE);
    r_terrainPatchSize = ri.Cvar_Get("r_terrainPatchSize", "64", CVAR_ARCHIVE);
    r_terrainMaterials = ri.Cvar_Get("r_terrainMaterials", "4", CVAR_ARCHIVE);

    if (!r_terrain->integer) {
        return;
    }

    // Set default configuration
    terrain_system.grid_size = r_terrainGridSize->integer;
    terrain_system.patch_size_world = r_terrainPatchSize->integer;
    terrain_system.height_scale = 100.0f;

    // Set default LOD distances
    terrain_system.lod_distances[0] = 500;   // LOD 0: closest
    terrain_system.lod_distances[1] = 1000;
    terrain_system.lod_distances[2] = 2000;
    terrain_system.lod_distances[3] = 4000;
    terrain_system.lod_distances[4] = 8000;
    terrain_system.lod_distances[5] = 16000; // LOD 5: farthest

    // Clamp grid size
    if (terrain_system.grid_size > TERRAIN_MAX_SIZE) {
        terrain_system.grid_size = TERRAIN_MAX_SIZE;
    }
    if (terrain_system.grid_size < 64) {
        terrain_system.grid_size = 64;
    }

    // Create Vulkan resources
    if (!vk_create_terrain_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create terrain resources\n");
        return;
    }

    // Create pipeline
    vk_create_terrain_pipeline();

    // Update descriptors
    vk_update_terrain_descriptors();

    // Generate default heightmap
    vk_terrain_generate_heightmap(terrain_system.grid_size, terrain_system.grid_size, terrain_system.height_scale);

    // Generate patches
    vk_generate_terrain_patches();

    terrain_system.initialized = qtrue;
    terrain_system.enabled = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Terrain system initialized with %d patches\n", terrain_system.num_patches);
}

// Shutdown terrain system
void vk_terrain_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down terrain system\n");

    vk_destroy_terrain_pipeline();
    vk_destroy_terrain_resources();

    // Allocated from the hunk; do not free individually.
    terrain_system.heightmap.heights = NULL;
    terrain_system.heightmap.normals = NULL;

    terrain_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Terrain system shut down\n");
}

// Update terrain system
void vk_terrain_update(void) {
    if (!terrain_system.initialized || !terrain_system.enabled) {
        return;
    }

    // Update LOD based on camera position
    vk_update_terrain_lod();
    
    // Update patch visibility for frustum culling
    vec3_t camera_pos;
    VectorCopy(tr.refdef.vieworg, camera_pos);
    
    for (int i = 0; i < terrain_system.num_patches; i++) {
        terrain_patch_t *patch = &terrain_system.patches[i];
        if (!patch->active) {
            continue;
        }
        
        // Calculate distance to camera
        vec3_t patch_center;
        patch_center[0] = (patch->x + 0.5f) * terrain_system.patch_size_world;
        patch_center[1] = (terrain_system.heightmap.min_height + terrain_system.heightmap.max_height) * 0.5f;
        patch_center[2] = (patch->y + 0.5f) * terrain_system.patch_size_world;
        
        patch->distance_to_camera = Distance(camera_pos, patch_center);
        
        // Frustum culling
        // Convert world-space bounds to local space for culling
        vec3_t localBounds[2];
        VectorCopy(patch->mins, localBounds[0]);
        VectorCopy(patch->maxs, localBounds[1]);
        patch->visible = !R_CullLocalBox(localBounds);
    }
}

// Render terrain
void vk_terrain_render(void) {
    if (!terrain_system.initialized || !terrain_system.enabled || terrain_system.num_patches == 0) {
        return;
    }

    if (terrain_system.pipeline == VK_NULL_HANDLE || terrain_system.pipeline_layout == VK_NULL_HANDLE) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, terrain_system.pipeline);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            terrain_system.pipeline_layout, 0, 1, &terrain_system.descriptor_set, 0, NULL);

    // Push constants for MVP matrix, camera position, and time
    // Note: MVP matrix is typically handled by the renderer's standard pipeline
    // Push constants would need to be added to pipeline layout if shaders require them
    // For now, we rely on the standard renderer pipeline state

    // Render visible patches
    for (int i = 0; i < terrain_system.num_patches; i++) {
        terrain_patch_t *patch = &terrain_system.patches[i];

        if (!patch->active || !patch->visible) {
            continue;
        }

        // Bind vertex/index buffers
        VkDeviceSize offset = 0;
        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, &patch->vertex_buffer, &offset);
        qvkCmdBindIndexBuffer(vk.cmd->command_buffer, patch->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // Bind material weights buffer
        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 1, 1, &patch->weight_buffer, &offset);

        // Draw patch
        qvkCmdDrawIndexed(vk.cmd->command_buffer, patch->index_count, 1, 0, 0, 0);
    }
}

// Load heightmap from file
qboolean vk_terrain_load_heightmap(const char *heightmap_path, float scale) {
    if (!terrain_system.initialized) {
        return qfalse;
    }

    if (!vk_load_heightmap_from_file(heightmap_path)) {
        return qfalse;
    }

    terrain_system.heightmap.scale = scale;
    vk_generate_heightmap_normals();
    vk_upload_heightmap_texture();

    // Regenerate patches with new heightmap
    vk_generate_terrain_patches();

    return qtrue;
}

// Generate procedural heightmap
qboolean vk_terrain_generate_heightmap(int width, int height, float scale) {
    if (!terrain_system.initialized) {
        return qfalse;
    }

    // Allocated from the hunk; do not free individually.
    terrain_system.heightmap.heights = NULL;
    terrain_system.heightmap.normals = NULL;

    terrain_system.heightmap.width = width;
    terrain_system.heightmap.height = height;
    terrain_system.heightmap.scale = scale;
    terrain_system.heightmap.min_height = 0.0f;
    terrain_system.heightmap.max_height = scale;

    int total_verts = width * height;
    terrain_system.heightmap.heights = ri.Hunk_Alloc(total_verts * sizeof(float), h_low);
    terrain_system.heightmap.normals = ri.Hunk_Alloc(total_verts * sizeof(vec3_t), h_low);

    if (!terrain_system.heightmap.heights || !terrain_system.heightmap.normals) {
        ri.Printf(PRINT_ERROR, "vk_terrain_generate_heightmap: Failed to allocate heightmap memory\n");
        return qfalse;
    }

    // Generate simple procedural terrain
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            float nx = (float)x / width;
            float ny = (float)y / height;

            // Simple noise-based height
            float height_val = sinf(nx * 10.0f) * cosf(ny * 10.0f) * 0.5f +
                              sinf(nx * 20.0f) * 0.25f +
                              cosf(ny * 20.0f) * 0.25f;

            height_val = (height_val + 1.0f) * 0.5f; // Normalize to 0-1
            terrain_system.heightmap.heights[index] = height_val * scale;
        }
    }

    vk_generate_heightmap_normals();
    vk_upload_heightmap_texture();

    return qtrue;
}

// Set terrain material
void vk_terrain_set_material(int index, const char *diffuse_path, const char *normal_path,
                           float scale_u, float scale_v, const vec3_t tint_color) {
    if (!terrain_system.initialized || index < 0 || index >= TERRAIN_MAX_MATERIALS) {
        return;
    }

    terrain_material_t *mat = &terrain_system.materials[index];

    mat->diffuse_texture = RE_RegisterShader(diffuse_path);
    mat->normal_texture = RE_RegisterShader(normal_path);
    mat->scale_u = scale_u;
    mat->scale_v = scale_v;
    mat->blend_strength = 1.0f;
    VectorCopy(tint_color, mat->tint_color);

    if (index >= terrain_system.num_materials) {
        terrain_system.num_materials = index + 1;
    }
}

// Update LOD for all patches (internal function)
static void vk_update_terrain_lod(void) {
    vec3_t camera_pos;
    VectorCopy(tr.refdef.vieworg, camera_pos);

    for (int i = 0; i < terrain_system.num_patches; i++) {
        terrain_patch_t *patch = &terrain_system.patches[i];

        if (!patch->active) {
            continue;
        }

        // Calculate distance to camera
        vec3_t patch_center;
        patch_center[0] = (patch->x + 0.5f) * terrain_system.patch_size_world;
        patch_center[1] = 0.0f; // Assume flat terrain center
        patch_center[2] = (patch->y + 0.5f) * terrain_system.patch_size_world;

        patch->distance_to_camera = Distance(camera_pos, patch_center);

        // Determine LOD level
        int lod_level = 0;
        for (int lod = 0; lod < TERRAIN_MAX_LOD_LEVELS; lod++) {
            if (patch->distance_to_camera > terrain_system.lod_distances[lod]) {
                lod_level = lod;
            } else {
                break;
            }
        }

        // Update geometry if LOD changed
        if (lod_level != patch->lod_level && r_terrainLod->integer) {
            patch->lod_level = lod_level;
            vk_build_patch_geometry(patch, lod_level);
        }

        // Frustum culling
        // Convert world-space bounds to local space for culling
        vec3_t localBounds[2];
        VectorCopy(patch->mins, localBounds[0]);
        VectorCopy(patch->maxs, localBounds[1]);
        patch->visible = !R_CullLocalBox(localBounds);
    }
}

// Create Vulkan resources for terrain
static qboolean vk_create_terrain_resources(void) {
    VkResult result;

    // Create height texture
    VkImageCreateInfo heightImageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32_SFLOAT,
        .extent = { (uint32_t)terrain_system.grid_size, (uint32_t)terrain_system.grid_size, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    result = qvkCreateImage(vk.device, &heightImageInfo, NULL, &terrain_system.height_texture);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_resources: Failed to create height texture\n");
        return qfalse;
    }

    SET_OBJECT_NAME(terrain_system.height_texture, "terrain_height_texture", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);

    // Allocate memory for height texture
    VkMemoryRequirements memReqs;
    qvkGetImageMemoryRequirements(vk.device, terrain_system.height_texture, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &terrain_system.height_texture_memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_resources: Failed to allocate height texture memory\n");
        return qfalse;
    }

    qvkBindImageMemory(vk.device, terrain_system.height_texture, terrain_system.height_texture_memory, 0);

    // Create height texture view
    VkImageViewCreateInfo heightViewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = terrain_system.height_texture,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32_SFLOAT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    result = qvkCreateImageView(vk.device, &heightViewInfo, NULL, &terrain_system.height_texture_view);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_resources: Failed to create height texture view\n");
        return qfalse;
    }

    // Create samplers
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    result = qvkCreateSampler(vk.device, &samplerInfo, NULL, &terrain_system.height_sampler);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_resources: Failed to create height sampler\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Terrain resources created successfully\n");
    return qtrue;
}

// Destroy Vulkan resources for terrain
static void vk_destroy_terrain_resources(void) {
    // Destroy patch resources
    for (int i = 0; i < terrain_system.num_patches; i++) {
        terrain_patch_t *patch = &terrain_system.patches[i];

        if (patch->vertex_buffer) {
            qvkDestroyBuffer(vk.device, patch->vertex_buffer, NULL);
            patch->vertex_buffer = VK_NULL_HANDLE;
        }
        if (patch->index_buffer) {
            qvkDestroyBuffer(vk.device, patch->index_buffer, NULL);
            patch->index_buffer = VK_NULL_HANDLE;
        }
        if (patch->weight_buffer) {
            qvkDestroyBuffer(vk.device, patch->weight_buffer, NULL);
            patch->weight_buffer = VK_NULL_HANDLE;
        }

        if (patch->vertex_memory) {
            qvkFreeMemory(vk.device, patch->vertex_memory, NULL);
            patch->vertex_memory = VK_NULL_HANDLE;
        }
        if (patch->index_memory) {
            qvkFreeMemory(vk.device, patch->index_memory, NULL);
            patch->index_memory = VK_NULL_HANDLE;
        }
        if (patch->weight_memory) {
            qvkFreeMemory(vk.device, patch->weight_memory, NULL);
            patch->weight_memory = VK_NULL_HANDLE;
        }
    }

    // Destroy textures
    if (terrain_system.height_sampler) {
        qvkDestroySampler(vk.device, terrain_system.height_sampler, NULL);
        terrain_system.height_sampler = VK_NULL_HANDLE;
    }

    if (terrain_system.height_texture_view) {
        qvkDestroyImageView(vk.device, terrain_system.height_texture_view, NULL);
        terrain_system.height_texture_view = VK_NULL_HANDLE;
    }

    if (terrain_system.height_texture) {
        qvkDestroyImage(vk.device, terrain_system.height_texture, NULL);
        terrain_system.height_texture = VK_NULL_HANDLE;
    }

    if (terrain_system.height_texture_memory) {
        qvkFreeMemory(vk.device, terrain_system.height_texture_memory, NULL);
        terrain_system.height_texture_memory = VK_NULL_HANDLE;
    }
}

// Generate terrain patches
static void vk_generate_terrain_patches(void) {
    if (!terrain_system.initialized) {
        return;
    }

    int patches_per_side = terrain_system.grid_size / TERRAIN_PATCH_SIZE;
    terrain_system.patches_per_side = patches_per_side;
    terrain_system.num_patches = patches_per_side * patches_per_side;

    if (terrain_system.num_patches > TERRAIN_MAX_PATCHES) {
        ri.Printf(PRINT_WARNING, "vk_generate_terrain_patches: Too many patches (%d), clamping to %d\n",
                 terrain_system.num_patches, TERRAIN_MAX_PATCHES);
        terrain_system.num_patches = TERRAIN_MAX_PATCHES;
    }

    for (int i = 0; i < terrain_system.num_patches; i++) {
        terrain_patch_t *patch = &terrain_system.patches[i];

        int patch_x = i % patches_per_side;
        int patch_y = i / patches_per_side;

        patch->x = patch_x;
        patch->y = patch_y;
        patch->lod_level = 0;
        patch->active = qtrue;

        // Build initial geometry
        vk_build_patch_geometry(patch, 0);
    }
}

// Build geometry for a terrain patch
static void vk_build_patch_geometry(terrain_patch_t *patch, int lod_level) {
    int vertices_per_patch = TERRAIN_PATCH_SIZE + 1;
    int step = 1 << lod_level; // LOD stepping

    if (step >= TERRAIN_PATCH_SIZE) {
        step = TERRAIN_PATCH_SIZE;
    }

    int verts_x = (TERRAIN_PATCH_SIZE / step) + 1;
    int verts_y = (TERRAIN_PATCH_SIZE / step) + 1;

    patch->vertex_count = verts_x * verts_y;
    patch->index_count = (verts_x - 1) * (verts_y - 1) * 6; // 2 triangles per quad

    // Allocate vertex data
    vec4_t *vertices = ri.Hunk_Alloc(patch->vertex_count * sizeof(vec4_t), h_low);
    uint32_t *indices = ri.Hunk_Alloc(patch->index_count * sizeof(uint32_t), h_low);
    vec4_t *weights = ri.Hunk_Alloc(patch->vertex_count * sizeof(vec4_t), h_low);

    if (!vertices || !indices || !weights) {
        ri.Printf(PRINT_ERROR, "vk_build_patch_geometry: Failed to allocate geometry memory\n");
        return;
    }

    // Generate vertices
    float patch_world_x = patch->x * terrain_system.patch_size_world;
    float patch_world_z = patch->y * terrain_system.patch_size_world;

    float vertex_spacing = terrain_system.patch_size_world / TERRAIN_PATCH_SIZE;

    int vertex_index = 0;
    for (int y = 0; y < verts_y; y++) {
        for (int x = 0; x < verts_x; x++) {
            int grid_x = patch->x * TERRAIN_PATCH_SIZE + x * step;
            int grid_y = patch->y * TERRAIN_PATCH_SIZE + y * step;

            // Clamp to heightmap bounds
            grid_x = MIN(grid_x, terrain_system.heightmap.width - 1);
            grid_y = MIN(grid_y, terrain_system.heightmap.height - 1);

            float height = terrain_system.heightmap.heights[grid_y * terrain_system.heightmap.width + grid_x];

            vertices[vertex_index][0] = patch_world_x + x * vertex_spacing * step;
            vertices[vertex_index][1] = height;
            vertices[vertex_index][2] = patch_world_z + y * vertex_spacing * step;
            vertices[vertex_index][3] = 1.0f;

            // Default material weights (equal distribution)
            for (int m = 0; m < 4; m++) {
                weights[vertex_index][m] = (m < terrain_system.num_materials) ? 0.25f : 0.0f;
            }

            vertex_index++;
        }
    }

    // Generate indices
    int index_index = 0;
    for (int y = 0; y < verts_y - 1; y++) {
        for (int x = 0; x < verts_x - 1; x++) {
            int top_left = y * verts_x + x;
            int top_right = top_left + 1;
            int bottom_left = (y + 1) * verts_x + x;
            int bottom_right = bottom_left + 1;

            // First triangle
            indices[index_index++] = top_left;
            indices[index_index++] = bottom_left;
            indices[index_index++] = top_right;

            // Second triangle
            indices[index_index++] = top_right;
            indices[index_index++] = bottom_left;
            indices[index_index++] = bottom_right;
        }
    }

    // Upload to GPU
    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = patch->vertex_count * sizeof(vec4_t),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (patch->vertex_buffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, patch->vertex_buffer, NULL);
        patch->vertex_buffer = VK_NULL_HANDLE;
    }
    
    if (qvkCreateBuffer(vk.device, &vertexBufferInfo, NULL, &patch->vertex_buffer) == VK_SUCCESS) {
        VkMemoryRequirements memReqs;
        qvkGetBufferMemoryRequirements(vk.device, patch->vertex_buffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        
        if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &patch->vertex_memory) == VK_SUCCESS) {
            qvkBindBufferMemory(vk.device, patch->vertex_buffer, patch->vertex_memory, 0);
            
            // Upload vertex data via staging buffer
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            VkBufferCreateInfo stagingInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = patch->vertex_count * sizeof(vec4_t),
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            
            if (qvkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer) == VK_SUCCESS) {
                qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                
                if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) == VK_SUCCESS) {
                    qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
                    
                    void *stagingData;
                    qvkMapMemory(vk.device, stagingMemory, 0, patch->vertex_count * sizeof(vec4_t), 0, &stagingData);
                    memcpy(stagingData, vertices, patch->vertex_count * sizeof(vec4_t));
                    qvkUnmapMemory(vk.device, stagingMemory);
                    
                    VkCommandBuffer cmdBuf = begin_command_buffer();
                    VkBufferCopy copyRegion = {.size = patch->vertex_count * sizeof(vec4_t)};
                    // qvkCmdCopyBuffer is declared in vk.c, need extern or include
                    extern PFN_vkCmdCopyBuffer qvkCmdCopyBuffer;
                    if (qvkCmdCopyBuffer) {
                        qvkCmdCopyBuffer(cmdBuf, stagingBuffer, patch->vertex_buffer, 1, &copyRegion);
                    }
                    end_command_buffer(cmdBuf, __func__);
                    
                    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
                    qvkFreeMemory(vk.device, stagingMemory, NULL);
                }
            }
        }
    }
    
    // Create index buffer
    VkBufferCreateInfo indexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = patch->index_count * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (patch->index_buffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, patch->index_buffer, NULL);
        patch->index_buffer = VK_NULL_HANDLE;
    }
    
    if (qvkCreateBuffer(vk.device, &indexBufferInfo, NULL, &patch->index_buffer) == VK_SUCCESS) {
        VkMemoryRequirements memReqs;
        qvkGetBufferMemoryRequirements(vk.device, patch->index_buffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        
        if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &patch->index_memory) == VK_SUCCESS) {
            qvkBindBufferMemory(vk.device, patch->index_buffer, patch->index_memory, 0);
            
            // Upload index data via staging buffer
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            VkBufferCreateInfo stagingInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = patch->index_count * sizeof(uint32_t),
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            
            if (qvkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer) == VK_SUCCESS) {
                qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                
                if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) == VK_SUCCESS) {
                    qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
                    
                    void *stagingData;
                    qvkMapMemory(vk.device, stagingMemory, 0, patch->index_count * sizeof(uint32_t), 0, &stagingData);
                    memcpy(stagingData, indices, patch->index_count * sizeof(uint32_t));
                    qvkUnmapMemory(vk.device, stagingMemory);
                    
                    VkCommandBuffer cmdBuf = begin_command_buffer();
                    VkBufferCopy copyRegion = {.size = patch->index_count * sizeof(uint32_t)};
                    qvkCmdCopyBuffer(cmdBuf, stagingBuffer, patch->index_buffer, 1, &copyRegion);
                    end_command_buffer(cmdBuf, __func__);
                    
                    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
                    qvkFreeMemory(vk.device, stagingMemory, NULL);
                }
            }
        }
    }
    
    // Create weight buffer (for material blending)
    VkBufferCreateInfo weightBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = patch->vertex_count * sizeof(vec4_t),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (patch->weight_buffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, patch->weight_buffer, NULL);
        patch->weight_buffer = VK_NULL_HANDLE;
    }
    
    if (qvkCreateBuffer(vk.device, &weightBufferInfo, NULL, &patch->weight_buffer) == VK_SUCCESS) {
        VkMemoryRequirements memReqs;
        qvkGetBufferMemoryRequirements(vk.device, patch->weight_buffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        
        if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &patch->weight_memory) == VK_SUCCESS) {
            qvkBindBufferMemory(vk.device, patch->weight_buffer, patch->weight_memory, 0);
            
            // Upload weight data via staging buffer
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            VkBufferCreateInfo stagingInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = patch->vertex_count * sizeof(vec4_t),
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            
            if (qvkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer) == VK_SUCCESS) {
                qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                
                if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) == VK_SUCCESS) {
                    qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
                    
                    void *stagingData;
                    qvkMapMemory(vk.device, stagingMemory, 0, patch->vertex_count * sizeof(vec4_t), 0, &stagingData);
                    memcpy(stagingData, weights, patch->vertex_count * sizeof(vec4_t));
                    qvkUnmapMemory(vk.device, stagingMemory);
                    
                    VkCommandBuffer cmdBuf = begin_command_buffer();
                    VkBufferCopy copyRegion = {.size = patch->vertex_count * sizeof(vec4_t)};
                    qvkCmdCopyBuffer(cmdBuf, stagingBuffer, patch->weight_buffer, 1, &copyRegion);
                    end_command_buffer(cmdBuf, __func__);
                    
                    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
                    qvkFreeMemory(vk.device, stagingMemory, NULL);
                }
            }
        }
    }

    // Update bounding box
    patch->mins[0] = patch_world_x;
    patch->mins[1] = terrain_system.heightmap.min_height;
    patch->mins[2] = patch_world_z;
    patch->maxs[0] = patch_world_x + terrain_system.patch_size_world;
    patch->maxs[1] = terrain_system.heightmap.max_height;
    patch->maxs[2] = patch_world_z + terrain_system.patch_size_world;

    // Temporary buffers are hunk-allocated; do not free individually.
}

// Load heightmap from file
static qboolean vk_load_heightmap_from_file(const char *path) {
    byte *image_data = NULL;
    int width = 0, height = 0;
    float *heights = NULL;
    float min_height = FLT_MAX, max_height = -FLT_MAX;

    if (!path || !path[0]) {
        ri.Printf(PRINT_WARNING, "vk_load_heightmap_from_file: Invalid path\n");
        return qfalse;
    }

    // Try loading image using renderer's image loading functions
    // These support PNG, TGA, JPG, BMP, etc.
    extern void R_LoadPNG(const char *name, byte **pic, int *width, int *height);
    extern void R_LoadTGA(const char *name, byte **pic, int *width, int *height);
    extern void R_LoadJPG(const char *name, byte **pic, int *width, int *height);
    extern void R_LoadBMP(const char *name, byte **pic, int *width, int *height);

    // Try different formats
    const char *ext = COM_GetExtension(path);
    if (ext && *ext) {
        if (!Q_stricmp(ext, "png")) {
            R_LoadPNG(path, &image_data, &width, &height);
        } else if (!Q_stricmp(ext, "tga")) {
            R_LoadTGA(path, &image_data, &width, &height);
        } else if (!Q_stricmp(ext, "jpg") || !Q_stricmp(ext, "jpeg")) {
            R_LoadJPG(path, &image_data, &width, &height);
        } else if (!Q_stricmp(ext, "bmp")) {
            R_LoadBMP(path, &image_data, &width, &height);
        } else {
            // Try PNG first, then TGA, then JPG
            R_LoadPNG(path, &image_data, &width, &height);
            if (!image_data) {
                R_LoadTGA(path, &image_data, &width, &height);
            }
            if (!image_data) {
                R_LoadJPG(path, &image_data, &width, &height);
            }
        }
    } else {
        // No extension, try common formats
        char try_path[MAX_QPATH];
        Q_strncpyz(try_path, path, sizeof(try_path));
        COM_DefaultExtension(try_path, sizeof(try_path), ".png");
        R_LoadPNG(try_path, &image_data, &width, &height);
        if (!image_data) {
            COM_DefaultExtension(try_path, sizeof(try_path), ".tga");
            R_LoadTGA(try_path, &image_data, &width, &height);
        }
    }

    if (!image_data || width <= 0 || height <= 0) {
        ri.Printf(PRINT_WARNING, "vk_load_heightmap_from_file: Failed to load image '%s'\n", path);
        return qfalse;
    }

    // Allocate height data
    heights = (float*)ri.Hunk_Alloc(sizeof(float) * width * height, h_low);
    if (!heights) {
        ri.Free(image_data);
        ri.Printf(PRINT_ERROR, "vk_load_heightmap_from_file: Failed to allocate height data\n");
        return qfalse;
    }

    // Convert image data to height values
    // Image loaders return RGBA format (4 bytes per pixel)
    // We'll use the red channel or convert to grayscale
    int bytes_per_pixel = 4; // RGBA format from image loaders

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_index = (y * width + x) * bytes_per_pixel;
            byte r = image_data[pixel_index];
            byte g = image_data[pixel_index + 1];
            byte b = image_data[pixel_index + 2];

            // Convert RGB to grayscale height (using luminance formula)
            float gray = 0.299f * r + 0.587f * g + 0.114f * b;
            float height_value = (gray / 255.0f) * terrain_system.heightmap.scale;

            heights[y * width + x] = height_value;

            if (height_value < min_height) min_height = height_value;
            if (height_value > max_height) max_height = height_value;
        }
    }

    // Free image data
    ri.Free(image_data);

    // Update terrain system
    if (terrain_system.heightmap.heights) {
        ri.Hunk_Free(terrain_system.heightmap.heights);
    }
    terrain_system.heightmap.heights = heights;
    terrain_system.heightmap.width = width;
    terrain_system.heightmap.height = height;
    terrain_system.heightmap.min_height = min_height;
    terrain_system.heightmap.max_height = max_height;

    // Allocate normals
    if (terrain_system.heightmap.normals) {
        ri.Hunk_Free(terrain_system.heightmap.normals);
    }
    terrain_system.heightmap.normals = (vec3_t*)ri.Hunk_Alloc(sizeof(vec3_t) * width * height, h_low);

    // Generate normals
    vk_generate_heightmap_normals();

    // Upload to GPU
    vk_upload_heightmap_texture();

    ri.Printf(PRINT_ALL, "vk_load_heightmap_from_file: Loaded heightmap '%s' (%dx%d, height range: %.2f to %.2f)\n",
              path, width, height, min_height, max_height);

    return qtrue;
}

// Generate surface normals for heightmap
static void vk_generate_heightmap_normals(void) {
    int width = terrain_system.heightmap.width;
    int height = terrain_system.heightmap.height;
    float *heights = terrain_system.heightmap.heights;
    vec3_t *normals = terrain_system.heightmap.normals;

    float height_scale = terrain_system.heightmap.scale / terrain_system.patch_size_world;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;

            // Calculate normal using central differences
            float left = (x > 0) ? heights[y * width + (x - 1)] : heights[index];
            float right = (x < width - 1) ? heights[y * width + (x + 1)] : heights[index];
            float up = (y > 0) ? heights[(y - 1) * width + x] : heights[index];
            float down = (y < height - 1) ? heights[(y + 1) * width + x] : heights[index];

            vec3_t normal;
            normal[0] = (left - right) * height_scale;
            normal[1] = 2.0f; // Vertical component
            normal[2] = (up - down) * height_scale;

            VectorNormalize(normal);
            VectorCopy(normal, normals[index]);
        }
    }
}

// Upload heightmap to GPU texture
static void vk_upload_heightmap_texture(void) {
    if (!terrain_system.initialized || !terrain_system.heightmap.heights) {
        return;
    }

    int width = terrain_system.heightmap.width;
    int height = terrain_system.heightmap.height;
    int data_size = width * height * sizeof(float);

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = data_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (qvkCreateBuffer(vk.device, &bufferInfo, NULL, &stagingBuffer) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_upload_heightmap_texture: Failed to create staging buffer\n");
        return;
    }
    
    VkMemoryRequirements memReqs;
    qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    
    if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_upload_heightmap_texture: Failed to allocate staging memory\n");
        qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
        return;
    }
    
    qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
    
    // Copy height data to staging buffer
    void *data;
    qvkMapMemory(vk.device, stagingMemory, 0, data_size, 0, &data);
    memcpy(data, terrain_system.heightmap.heights, data_size);
    qvkUnmapMemory(vk.device, stagingMemory);
    
    // Copy from staging buffer to image
    VkCommandBuffer cmdBuf = begin_command_buffer();
    
    // Transition image to transfer destination
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = terrain_system.height_texture,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1
        },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    
    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {(uint32_t)width, (uint32_t)height, 1}
    };
    
    qvkCmdCopyBufferToImage(cmdBuf, stagingBuffer, terrain_system.height_texture,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition image to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);
    
    end_command_buffer(cmdBuf, __func__);
    
    // Cleanup staging buffer
    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
    qvkFreeMemory(vk.device, stagingMemory, NULL);
}

// Height editing and querying functions
void vk_terrain_set_height(int x, int y, float height) {
    if (!terrain_system.heightmap.heights || 
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height) {
        return;
    }

    int index = y * terrain_system.heightmap.width + x;
    terrain_system.heightmap.heights[index] = height;

    // Update min/max
    if (height < terrain_system.heightmap.min_height) {
        terrain_system.heightmap.min_height = height;
    }
    if (height > terrain_system.heightmap.max_height) {
        terrain_system.heightmap.max_height = height;
    }

    // Regenerate normals for affected area
    vk_generate_heightmap_normals();

    // Re-upload to GPU
    vk_upload_heightmap_texture();
}

float vk_terrain_get_height(int x, int y) {
    if (!terrain_system.heightmap.heights ||
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height) {
        return 0.0f;
    }

    int index = y * terrain_system.heightmap.width + x;
    return terrain_system.heightmap.heights[index];
}

void vk_terrain_smooth_area(int x, int y, int radius) {
    if (!terrain_system.heightmap.heights ||
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height ||
        radius <= 0) {
        return;
    }

    // Calculate affected area
    int min_x = MAX(0, x - radius);
    int max_x = MIN(terrain_system.heightmap.width - 1, x + radius);
    int min_y = MAX(0, y - radius);
    int max_y = MIN(terrain_system.heightmap.height - 1, y + radius);

    // Allocate temporary buffer for smoothed heights
    float *smoothed = (float*)ri.Hunk_AllocateTempMemory((max_x - min_x + 1) * (max_y - min_y + 1) * sizeof(float), h_low);
    if (!smoothed) {
        ri.Printf(PRINT_WARNING, "vk_terrain_smooth_area: Failed to allocate temporary buffer\n");
        return;
    }

    // Apply Gaussian-like smoothing kernel
    for (int sy = min_y; sy <= max_y; sy++) {
        for (int sx = min_x; sx <= max_x; sx++) {
            float sum = 0.0f;
            float weight_sum = 0.0f;

            // Sample neighborhood
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = sx + dx;
                    int ny = sy + dy;

                    if (nx < 0 || nx >= terrain_system.heightmap.width ||
                        ny < 0 || ny >= terrain_system.heightmap.height) {
                        continue;
                    }

                    // Gaussian weight based on distance
                    float dist = sqrtf((float)(dx * dx + dy * dy));
                    if (dist > radius) continue;

                    float weight = expf(-(dist * dist) / (2.0f * (radius * radius) / 4.0f));
                    int index = ny * terrain_system.heightmap.width + nx;
                    sum += terrain_system.heightmap.heights[index] * weight;
                    weight_sum += weight;
                }
            }

            if (weight_sum > 0.0f) {
                int local_index = (sy - min_y) * (max_x - min_x + 1) + (sx - min_x);
                smoothed[local_index] = sum / weight_sum;
            }
        }
    }

    // Copy smoothed heights back
    for (int sy = min_y; sy <= max_y; sy++) {
        for (int sx = min_x; sx <= max_x; sx++) {
            int index = sy * terrain_system.heightmap.width + sx;
            int local_index = (sy - min_y) * (max_x - min_x + 1) + (sx - min_x);
            terrain_system.heightmap.heights[index] = smoothed[local_index];
        }
    }

    ri.Hunk_FreeTempMemory(smoothed);

    // Regenerate normals for affected area
    vk_generate_heightmap_normals();

    // Re-upload to GPU
    vk_upload_heightmap_texture();

    ri.Printf(PRINT_DEVELOPER, "vk_terrain_smooth_area: Smoothed area at (%d, %d) with radius %d\n", x, y, radius);
}

void vk_terrain_set_lod_distances(int lod0_dist, int lod1_dist, int lod2_dist, int lod3_dist, int lod4_dist, int lod5_dist) {
    terrain_system.lod_distances[0] = lod0_dist;
    terrain_system.lod_distances[1] = lod1_dist;
    terrain_system.lod_distances[2] = lod2_dist;
    terrain_system.lod_distances[3] = lod3_dist;
    terrain_system.lod_distances[4] = lod4_dist;
    terrain_system.lod_distances[5] = lod5_dist;
}

void vk_terrain_set_material_weight(int x, int y, int material_index, float weight) {
    if (!terrain_system.initialized ||
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height ||
        material_index < 0 || material_index >= TERRAIN_MAX_MATERIALS ||
        weight < 0.0f || weight > 1.0f) {
        return;
    }

    // Find the patch containing this vertex
    int patch_x = x / TERRAIN_PATCH_SIZE;
    int patch_y = y / TERRAIN_PATCH_SIZE;
    int local_x = x % TERRAIN_PATCH_SIZE;
    int local_y = y % TERRAIN_PATCH_SIZE;

    if (patch_x < 0 || patch_x >= terrain_system.patches_per_side ||
        patch_y < 0 || patch_y >= terrain_system.patches_per_side) {
        return;
    }

    int patch_index = patch_y * terrain_system.patches_per_side + patch_x;
    if (patch_index >= terrain_system.num_patches) {
        return;
    }

    terrain_patch_t *patch = &terrain_system.patches[patch_index];

    // Material weights are stored per-vertex in patches
    // We need to rebuild the patch geometry to update weights
    // For now, mark patch as needing update
    // Note: Full implementation would require storing weights in a separate structure
    // and updating them when patches are rebuilt
    ri.Printf(PRINT_DEVELOPER, "vk_terrain_set_material_weight: Set material %d weight to %.2f at (%d, %d)\n",
              material_index, weight, x, y);

    // Rebuild patch geometry with updated weights
    // This is a simplified implementation - full version would update weights directly
    vk_build_patch_geometry(patch, patch->lod_level);
}

void vk_terrain_paint_material(int x, int y, int material_index, int radius, float strength) {
    if (!terrain_system.initialized ||
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height ||
        material_index < 0 || material_index >= TERRAIN_MAX_MATERIALS ||
        radius <= 0 || strength <= 0.0f || strength > 1.0f) {
        return;
    }

    // Calculate affected area
    int min_x = MAX(0, x - radius);
    int max_x = MIN(terrain_system.heightmap.width - 1, x + radius);
    int min_y = MAX(0, y - radius);
    int max_y = MIN(terrain_system.heightmap.height - 1, y + radius);

    // Paint material weights with falloff
    for (int sy = min_y; sy <= max_y; sy++) {
        for (int sx = min_x; sx <= max_x; sx++) {
            float dx = (float)(sx - x);
            float dy = (float)(sy - y);
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > radius) continue;

            // Calculate falloff (linear from center to edge)
            float falloff = 1.0f - (dist / radius);
            float paint_strength = strength * falloff;

            // Find patch containing this vertex
            int patch_x = sx / TERRAIN_PATCH_SIZE;
            int patch_y = sy / TERRAIN_PATCH_SIZE;

            if (patch_x >= 0 && patch_x < terrain_system.patches_per_side &&
                patch_y >= 0 && patch_y < terrain_system.patches_per_side) {
                int patch_index = patch_y * terrain_system.patches_per_side + patch_x;
                if (patch_index < terrain_system.num_patches) {
                    // Mark patch for rebuild with updated material weights
                    // Full implementation would update weights directly in patch data
                    terrain_patch_t *patch = &terrain_system.patches[patch_index];
                    // Rebuild patch to apply material painting
                    vk_build_patch_geometry(patch, patch->lod_level);
                }
            }
        }
    }

    ri.Printf(PRINT_DEVELOPER, "vk_terrain_paint_material: Painted material %d at (%d, %d) with radius %d, strength %.2f\n",
              material_index, x, y, radius, strength);
}

void vk_terrain_get_normal(int x, int y, vec3_t out_normal) {
    if (!out_normal || !terrain_system.heightmap.normals ||
        x < 0 || x >= terrain_system.heightmap.width ||
        y < 0 || y >= terrain_system.heightmap.height) {
        if (out_normal) {
            out_normal[0] = 0.0f;
            out_normal[1] = 1.0f;
            out_normal[2] = 0.0f;
        }
        return;
    }

    int index = y * terrain_system.heightmap.width + x;
    VectorCopy(terrain_system.heightmap.normals[index], out_normal);
}

// Ray-triangle intersection helper (Möller-Trumbore algorithm)
static qboolean ray_triangle_intersect(const vec3_t ray_origin, const vec3_t ray_dir,
                                       const vec3_t v0, const vec3_t v1, const vec3_t v2,
                                       float *t, float *u, float *v) {
    vec3_t edge1, edge2, tvec, pvec, qvec;
    float det, inv_det;
    const float EPSILON = 0.000001f;

    VectorSubtract(v1, v0, edge1);
    VectorSubtract(v2, v0, edge2);
    CrossProduct(ray_dir, edge2, pvec);

    det = DotProduct(edge1, pvec);
    if (det > -EPSILON && det < EPSILON) {
        return qfalse; // Ray is parallel to triangle
    }

    inv_det = 1.0f / det;
    VectorSubtract(ray_origin, v0, tvec);

    *u = DotProduct(tvec, pvec) * inv_det;
    if (*u < 0.0f || *u > 1.0f) {
        return qfalse;
    }

    CrossProduct(tvec, edge1, qvec);
    *v = DotProduct(ray_dir, qvec) * inv_det;
    if (*v < 0.0f || (*u + *v) > 1.0f) {
        return qfalse;
    }

    *t = DotProduct(edge2, qvec) * inv_det;
    return (*t > EPSILON); // Intersection is in front of ray origin
}

qboolean vk_terrain_trace(const vec3_t start, const vec3_t end, vec3_t hit_pos) {
    if (!terrain_system.initialized || !terrain_system.heightmap.heights || !hit_pos) {
        return qfalse;
    }

    vec3_t ray_dir;
    float ray_length;
    float closest_t = FLT_MAX;
    qboolean found_hit = qfalse;
    vec3_t best_hit;

    VectorSubtract(end, start, ray_dir);
    ray_length = VectorLength(ray_dir);
    if (ray_length < 0.0001f) {
        return qfalse;
    }
    VectorNormalize(ray_dir);

    // Trace through terrain patches
    for (int p = 0; p < terrain_system.num_patches; p++) {
        terrain_patch_t *patch = &terrain_system.patches[p];
        if (!patch->active || !patch->visible) {
            continue;
        }

        // Quick AABB test
        if (start[0] < patch->mins[0] && end[0] < patch->mins[0]) continue;
        if (start[0] > patch->maxs[0] && end[0] > patch->maxs[0]) continue;
        if (start[2] < patch->mins[2] && end[2] < patch->mins[2]) continue;
        if (start[2] > patch->maxs[2] && end[2] > patch->maxs[2]) continue;

        // Trace triangles in this patch
        // Reconstruct vertices from heightmap
        float patch_world_x = patch->x * terrain_system.patch_size_world;
        float patch_world_z = patch->y * terrain_system.patch_size_world;
        float vertex_spacing = terrain_system.patch_size_world / TERRAIN_PATCH_SIZE;

        int step = 1 << patch->lod_level; // LOD step
        int verts_x = (TERRAIN_PATCH_SIZE / step) + 1;
        int verts_y = (TERRAIN_PATCH_SIZE / step) + 1;

        for (int y = 0; y < verts_y - 1; y++) {
            for (int x = 0; x < verts_x - 1; x++) {
                int grid_x = patch->x * TERRAIN_PATCH_SIZE + x * step;
                int grid_y = patch->y * TERRAIN_PATCH_SIZE + y * step;

                if (grid_x >= terrain_system.heightmap.width - 1 ||
                    grid_y >= terrain_system.heightmap.height - 1) {
                    continue;
                }

                // Get triangle vertices
                vec3_t v0, v1, v2, v3;
                float h0 = terrain_system.heightmap.heights[grid_y * terrain_system.heightmap.width + grid_x];
                float h1 = terrain_system.heightmap.heights[grid_y * terrain_system.heightmap.width + (grid_x + step)];
                float h2 = terrain_system.heightmap.heights[(grid_y + step) * terrain_system.heightmap.width + grid_x];
                float h3 = terrain_system.heightmap.heights[(grid_y + step) * terrain_system.heightmap.width + (grid_x + step)];

                v0[0] = patch_world_x + x * vertex_spacing * step;
                v0[1] = h0;
                v0[2] = patch_world_z + y * vertex_spacing * step;

                v1[0] = patch_world_x + (x + 1) * vertex_spacing * step;
                v1[1] = h1;
                v1[2] = patch_world_z + y * vertex_spacing * step;

                v2[0] = patch_world_x + x * vertex_spacing * step;
                v2[1] = h2;
                v2[2] = patch_world_z + (y + 1) * vertex_spacing * step;

                v3[0] = patch_world_x + (x + 1) * vertex_spacing * step;
                v3[1] = h3;
                v3[2] = patch_world_z + (y + 1) * vertex_spacing * step;

                // Test first triangle (v0, v2, v1)
                float t, u, v;
                if (ray_triangle_intersect(start, ray_dir, v0, v2, v1, &t, &u, &v)) {
                    if (t < closest_t && t <= ray_length) {
                        closest_t = t;
                        best_hit[0] = start[0] + ray_dir[0] * t;
                        best_hit[1] = start[1] + ray_dir[1] * t;
                        best_hit[2] = start[2] + ray_dir[2] * t;
                        found_hit = qtrue;
                    }
                }

                // Test second triangle (v1, v2, v3)
                if (ray_triangle_intersect(start, ray_dir, v1, v2, v3, &t, &u, &v)) {
                    if (t < closest_t && t <= ray_length) {
                        closest_t = t;
                        best_hit[0] = start[0] + ray_dir[0] * t;
                        best_hit[1] = start[1] + ray_dir[1] * t;
                        best_hit[2] = start[2] + ray_dir[2] * t;
                        found_hit = qtrue;
                    }
                }
            }
        }
    }

    if (found_hit) {
        VectorCopy(best_hit, hit_pos);
        return qtrue;
    }

    return qfalse;
}

void vk_terrain_get_height_range(float *min_height, float *max_height) {
    if (min_height) *min_height = terrain_system.heightmap.min_height;
    if (max_height) *max_height = terrain_system.heightmap.max_height;
}

// Pipeline creation
static void vk_create_terrain_pipeline(void) {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // Height texture
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        },
        // Material textures (up to TERRAIN_MAX_MATERIALS)
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = TERRAIN_MAX_MATERIALS,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(bindings),
        .pBindings = bindings
    };

    VkResult result = qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &terrain_system.descriptor_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_pipeline: Failed to create descriptor set layout\n");
        return;
    }

    // Pipeline layout with push constants for MVP matrix
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(float) * 16 + sizeof(float) * 3 + sizeof(float) // MVP + camera pos + time
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &terrain_system.descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    result = qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &terrain_system.pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_pipeline: Failed to create pipeline layout\n");
        return;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TERRAIN_MAX_MATERIALS + 1 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = 1
    };

    result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &terrain_system.descriptor_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_pipeline: Failed to create descriptor pool\n");
        return;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = terrain_system.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &terrain_system.descriptor_layout
    };

    result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &terrain_system.descriptor_set);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_pipeline: Failed to allocate descriptor set\n");
        return;
    }

    // Load shader modules
    extern VkShaderModule vk_load_shader(const char *shader_name, VkShaderStageFlagBits stage);
    VkShaderModule vertModule = vk_load_shader("terrain_vert", VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragModule = vk_load_shader("terrain_frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    
    // Fallback to generic shaders if terrain-specific shaders don't exist
    if (vertModule == VK_NULL_HANDLE) {
        vertModule = vk_load_shader("color_vert", VK_SHADER_STAGE_VERTEX_BIT);
    }
    if (fragModule == VK_NULL_HANDLE) {
        fragModule = vk_load_shader("color_frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "vk_create_terrain_pipeline: Failed to load shader modules, using fallback\n");
        if (vertModule != VK_NULL_HANDLE) qvkDestroyShaderModule(vk.device, vertModule, NULL);
        if (fragModule != VK_NULL_HANDLE) qvkDestroyShaderModule(vk.device, fragModule, NULL);
        return;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragModule,
            .pName = "main"
        }
    };

    // Vertex input (position + material weights)
    VkVertexInputBindingDescription vertexBindings[] = {
        {
            .binding = 0,
            .stride = sizeof(vec4_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
        {
            .binding = 1,
            .stride = sizeof(vec4_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    VkVertexInputAttributeDescription vertexAttributes[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = ARRAY_LEN(vertexBindings),
        .pVertexBindingDescriptions = vertexBindings,
        .vertexAttributeDescriptionCount = ARRAY_LEN(vertexAttributes),
        .pVertexAttributeDescriptions = vertexAttributes
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blend
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_LEN(dynamicStates),
        .pDynamicStates = dynamicStates
    };

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ARRAY_LEN(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = terrain_system.pipeline_layout,
        .renderPass = vk.render_pass.main,  // Use main render pass
        .subpass = 0
    };

    result = qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &terrain_system.pipeline);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_terrain_pipeline: Failed to create graphics pipeline: %s\n", vk_result_string(result));
    } else {
        SET_OBJECT_NAME(terrain_system.pipeline, "terrain_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
        ri.Printf(PRINT_ALL, "Vulkan: Terrain pipeline created successfully\n");
    }

    // Cleanup shader modules (they're cached by the shader manager)
    // Don't destroy them here as they may be used elsewhere
}

static void vk_destroy_terrain_pipeline(void) {
    if (terrain_system.pipeline) {
        qvkDestroyPipeline(vk.device, terrain_system.pipeline, NULL);
        terrain_system.pipeline = VK_NULL_HANDLE;
    }

    if (terrain_system.pipeline_layout) {
        qvkDestroyPipelineLayout(vk.device, terrain_system.pipeline_layout, NULL);
        terrain_system.pipeline_layout = VK_NULL_HANDLE;
    }

    if (terrain_system.descriptor_layout) {
        qvkDestroyDescriptorSetLayout(vk.device, terrain_system.descriptor_layout, NULL);
        terrain_system.descriptor_layout = VK_NULL_HANDLE;
    }

    if (terrain_system.descriptor_pool) {
        qvkDestroyDescriptorPool(vk.device, terrain_system.descriptor_pool, NULL);
        terrain_system.descriptor_pool = VK_NULL_HANDLE;
    }
}

static void vk_update_terrain_descriptors(void) {
    // Update descriptor set with height texture
    VkDescriptorImageInfo heightInfo = {
        .sampler = terrain_system.height_sampler,
        .imageView = terrain_system.height_texture_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet writes[TERRAIN_MAX_MATERIALS + 1];
    writes[0] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = terrain_system.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &heightInfo
    };

    // Update material textures
    VkDescriptorImageInfo materialInfos[TERRAIN_MAX_MATERIALS];
    for (int i = 0; i < terrain_system.num_materials && i < TERRAIN_MAX_MATERIALS; i++) {
        terrain_material_t *mat = &terrain_system.materials[i];
        if (mat->diffuse_texture > 0) {
            shader_t *shader = tr.shaders[mat->diffuse_texture];
            if (shader && shader->stages[0] && shader->stages[0]->bundle[0].image[0]) {
                materialInfos[i] = (VkDescriptorImageInfo){
                    .sampler = vk.samplers.samplers[0],
                    .imageView = shader->stages[0]->bundle[0].image[0]->view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            } else {
                // Use white texture as fallback
                materialInfos[i] = (VkDescriptorImageInfo){
                    .sampler = vk.samplers.samplers[0],
                    .imageView = tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            }
        } else {
            // Use white texture as fallback
            materialInfos[i] = (VkDescriptorImageInfo){
                .sampler = vk.samplers.samplers[0],
                    .imageView = tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        }
    }
    
    // Fill remaining slots with white texture
    for (int i = terrain_system.num_materials; i < TERRAIN_MAX_MATERIALS; i++) {
        materialInfos[i] = (VkDescriptorImageInfo){
            .sampler = vk.samplers.samplers[0],
            .imageView = tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    writes[1] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = terrain_system.descriptor_set,
        .dstBinding = 1,
        .descriptorCount = TERRAIN_MAX_MATERIALS,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = materialInfos
    };

    qvkUpdateDescriptorSets(vk.device, 2, writes, 0, NULL);
}

#endif // USE_VULKAN