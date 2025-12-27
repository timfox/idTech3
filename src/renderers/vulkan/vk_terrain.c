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

    // TODO: Push constants / per-frame uniforms once the pipeline is implemented.

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

// Update LOD for all patches
void vk_terrain_update_lod(void) {
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
        patch->visible = R_CullBox(patch->mins, patch->maxs);
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
    // TODO: Implement vertex buffer creation and upload

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
    // TODO: Implement heightmap loading from various formats (PNG, TGA, etc.)
    ri.Printf(PRINT_WARNING, "vk_load_heightmap_from_file: Not implemented yet\n");
    return qfalse;
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

    // Upload height data
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

    vk_upload_image_data(NULL, 0, 0, width, height, 1, terrain_system.heightmap.heights, data_size, qfalse);
}

// Stub implementations for remaining functions
void vk_terrain_set_height(int x, int y, float height) {
    // TODO: Implement height editing
}

float vk_terrain_get_height(int x, int y) {
    // TODO: Implement height querying
    return 0.0f;
}

void vk_terrain_smooth_area(int x, int y, int radius) {
    // TODO: Implement area smoothing
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
    // TODO: Implement material weight editing
}

void vk_terrain_paint_material(int x, int y, int material_index, int radius, float strength) {
    // TODO: Implement material painting
}

void vk_terrain_get_normal(int x, int y, vec3_t out_normal) {
    (void)x; (void)y;
    if (!out_normal) {
        return;
    }
    // TODO: Return actual normal
    out_normal[0] = 0.0f;
    out_normal[1] = 1.0f;
    out_normal[2] = 0.0f;
}

qboolean vk_terrain_trace(const vec3_t start, const vec3_t end, vec3_t hit_pos) {
    // TODO: Implement terrain ray tracing
    return qfalse;
}

void vk_terrain_get_height_range(float *min_height, float *max_height) {
    if (min_height) *min_height = terrain_system.heightmap.min_height;
    if (max_height) *max_height = terrain_system.heightmap.max_height;
}

// Pipeline creation (stub)
static void vk_create_terrain_pipeline(void) {
    // TODO: Create terrain rendering pipeline
    ri.Printf(PRINT_WARNING, "vk_create_terrain_pipeline: Not implemented yet\n");
}

static void vk_destroy_terrain_pipeline(void) {
    // TODO: Destroy terrain pipeline
}

static void vk_update_terrain_descriptors(void) {
    // TODO: Update terrain descriptors
}

#endif // USE_VULKAN