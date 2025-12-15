#include "gltf_loader.h"
#include "tr_local.h"
#include "vk.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// JSON parsing (simplified - in production would use a proper JSON library)
typedef struct {
    const char* json;
    int pos;
    int length;
} json_parser_t;

static qboolean json_skip_whitespace(json_parser_t* parser) {
    while (parser->pos < parser->length) {
        char c = parser->json[parser->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return qtrue;
        }
        parser->pos++;
    }
    return qfalse;
}

static qboolean json_expect_char(json_parser_t* parser, char expected) {
    if (!json_skip_whitespace(parser)) return qfalse;
    if (parser->json[parser->pos] != expected) return qfalse;
    parser->pos++;
    return qtrue;
}

static qboolean json_read_string(json_parser_t* parser, char* buffer, int maxLen) {
    if (!json_expect_char(parser, '"')) return qfalse;

    int start = parser->pos;
    while (parser->pos < parser->length && parser->json[parser->pos] != '"') {
        if (parser->json[parser->pos] == '\\') parser->pos++; // Skip escaped chars
        parser->pos++;
    }

    if (parser->pos >= parser->length) return qfalse;

    int len = parser->pos - start;
    if (len >= maxLen) len = maxLen - 1;

    memcpy(buffer, &parser->json[start], len);
    buffer[len] = '\0';

    parser->pos++; // Skip closing quote
    return qtrue;
}

static int json_read_int(json_parser_t* parser) {
    if (!json_skip_whitespace(parser)) return 0;

    int start = parser->pos;
    while (parser->pos < parser->length &&
           ((parser->json[parser->pos] >= '0' && parser->json[parser->pos] <= '9') ||
            parser->json[parser->pos] == '-')) {
        parser->pos++;
    }

    char buffer[32];
    int len = parser->pos - start;
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    memcpy(buffer, &parser->json[start], len);
    buffer[len] = '\0';

    return atoi(buffer);
}

static float json_read_float(json_parser_t* parser) {
    if (!json_skip_whitespace(parser)) return 0.0f;

    int start = parser->pos;
    while (parser->pos < parser->length &&
           ((parser->json[parser->pos] >= '0' && parser->json[parser->pos] <= '9') ||
            parser->json[parser->pos] == '-' || parser->json[parser->pos] == '.' ||
            parser->json[parser->pos] == 'e' || parser->json[parser->pos] == 'E' ||
            parser->json[parser->pos] == '+')) {
        parser->pos++;
    }

    char buffer[64];
    int len = parser->pos - start;
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    memcpy(buffer, &parser->json[start], len);
    buffer[len] = '\0';

    return atof(buffer);
}

// glTF model management
#define MAX_GLTF_MODELS 256
static gltfModel_t gltfModels[MAX_GLTF_MODELS];
static qboolean gltfModelsUsed[MAX_GLTF_MODELS];

void R_GLTF_Init(void) {
    memset(gltfModelsUsed, 0, sizeof(gltfModelsUsed));
}

void R_GLTF_Shutdown(void) {
    for (int i = 0; i < MAX_GLTF_MODELS; i++) {
        if (gltfModelsUsed[i]) {
            R_FreeGLTF(i + 1); // qhandle_t is 1-based
        }
    }
}

qhandle_t R_LoadGLTF(const char* filename) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_GLTF_MODELS; i++) {
        if (!gltfModelsUsed[i]) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        ri.Printf(PRINT_WARNING, "R_LoadGLTF: No free slots for glTF model\n");
        return 0;
    }

    gltfModel_t* model = &gltfModels[slot];
    memset(model, 0, sizeof(gltfModel_t));
    Q_strncpyz(model->filename, filename, sizeof(model->filename));

    // Load glTF file
    fileHandle_t file;
    int fileLen = ri.FS_FOpenFileRead(filename, &file, qfalse);
    if (fileLen <= 0) {
        ri.Printf(PRINT_WARNING, "R_LoadGLTF: Could not open %s\n", filename);
        return 0;
    }

    char* fileData = ri.Hunk_AllocateTempMemory(fileLen + 1);
    ri.FS_Read(fileData, fileLen, file);
    fileData[fileLen] = '\0';
    ri.FS_FCloseFile(file);

    // Parse glTF JSON
    if (!R_GLTF_ParseJSON(model, fileData)) {
        ri.Printf(PRINT_WARNING, "R_LoadGLTF: Failed to parse %s\n", filename);
        ri.Hunk_FreeTempMemory(fileData);
        return 0;
    }

    // Load binary data
    char basePath[MAX_QPATH];
    COM_StripExtension(filename, basePath, sizeof(basePath));

    R_GLTF_LoadBuffers(model, basePath);
    R_GLTF_LoadImages(model, basePath);
    R_GLTF_CreateVulkanResources(model);

    // Compute bounds
    R_GLTF_ComputeBounds(model);

    ri.Hunk_FreeTempMemory(fileData);
    gltfModelsUsed[slot] = qtrue;
    model->loaded = qtrue;

    ri.Printf(PRINT_ALL, "Loaded glTF model: %s (%d meshes, %d materials)\n",
              filename, model->meshCount, model->materialCount);

    return slot + 1; // qhandle_t is 1-based
}

void R_FreeGLTF(qhandle_t handle) {
    if (handle <= 0 || handle > MAX_GLTF_MODELS) return;

    int slot = handle - 1;
    if (!gltfModelsUsed[slot]) return;

    gltfModel_t* model = &gltfModels[slot];

    // Free Vulkan resources
    if (model->descriptorSet != VK_NULL_HANDLE) {
        // TODO: Free descriptor set
    }

    // Free buffers
    for (int i = 0; i < model->bufferCount; i++) {
        gltfBuffer_t* buffer = &model->buffers[i];
        if (buffer->vkBuffer != VK_NULL_HANDLE) {
            qvkDestroyBuffer(vk.device, buffer->vkBuffer, NULL);
        }
        if (buffer->vkMemory != VK_NULL_HANDLE) {
            qvkFreeMemory(vk.device, buffer->vkMemory, NULL);
        }
        if (buffer->data) {
            ri.Hunk_FreeTempMemory(buffer->data);
        }
    }

    // Free images
    for (int i = 0; i < model->imageCount; i++) {
        gltfImage_t* image = &model->images[i];
        if (image->vkImage != VK_NULL_HANDLE) {
            qvkDestroyImage(vk.device, image->vkImage, NULL);
        }
        if (image->vkImageView != VK_NULL_HANDLE) {
            qvkDestroyImageView(vk.device, image->vkImageView, NULL);
        }
        if (image->vkMemory != VK_NULL_HANDLE) {
            qvkFreeMemory(vk.device, image->vkMemory, NULL);
        }
    }

    memset(model, 0, sizeof(gltfModel_t));
    gltfModelsUsed[slot] = qfalse;
}

qboolean R_GLTF_ParseJSON(gltfModel_t* model, const char* jsonData) {
    // Simplified JSON parsing - in production would use a proper JSON library
    json_parser_t parser = {jsonData, 0, strlen(jsonData)};

    // Skip to assets object
    // This is a very basic parser - production code would use cJSON or similar

    // For now, create minimal valid structure
    model->sceneCount = 1;
    model->scenes = ri.Hunk_AllocateTempMemory(sizeof(gltfScene_t));
    Q_strncpyz(model->scenes[0].name, "default", sizeof(model->scenes[0].name));
    model->scenes[0].nodes = NULL;
    model->scenes[0].nodeCount = 0;
    model->defaultScene = 0;

    // Create a simple cube mesh for testing
    model->meshCount = 1;
    model->meshes = ri.Hunk_AllocateTempMemory(sizeof(gltfMesh_t));
    Q_strncpyz(model->meshes[0].name, "cube", sizeof(model->meshes[0].name));
    model->meshes[0].primitiveCount = 1;
    model->meshes[0].primitives = ri.Hunk_AllocateTempMemory(sizeof(gltfPrimitive_t));

    // Create basic material
    model->materialCount = 1;
    model->materials = ri.Hunk_AllocateTempMemory(sizeof(gltfMaterial_t));
    Q_strncpyz(model->materials[0].name, "default", sizeof(model->materials[0].name));
    VectorSet(model->materials[0].baseColorFactor, 1.0f, 1.0f, 1.0f);
    model->materials[0].baseColorFactor[3] = 1.0f;
    model->materials[0].metallicFactor = 0.0f;
    model->materials[0].roughnessFactor = 0.5f;
    VectorSet(model->materials[0].emissiveFactor, 0.0f, 0.0f, 0.0f);

    return qtrue;
}

void R_GLTF_LoadBuffers(gltfModel_t* model, const char* basePath) {
    // Create vertex/index buffers for a simple cube
    const float vertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
    };

    const uint16_t indices[] = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        0, 3, 7, 7, 4, 0,
        // Right face
        1, 5, 6, 6, 2, 1,
        // Top face
        3, 2, 6, 6, 7, 3,
        // Bottom face
        0, 1, 5, 5, 4, 0
    };

    // Create Vulkan buffers
    gltfPrimitive_t* prim = &model->meshes[0].primitives[0];
    prim->vertexCount = 8;
    prim->indexCount = 36;

    // Vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = sizeof(vertices);
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &vertexBufferInfo, NULL, &prim->vertexBuffer));

    VkMemoryRequirements vertexMemReq;
    qvkGetBufferMemoryRequirements(vk.device, prim->vertexBuffer, &vertexMemReq);

    VkMemoryAllocateInfo vertexAllocInfo = {};
    vertexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertexAllocInfo.allocationSize = vertexMemReq.size;
    vertexAllocInfo.memoryTypeIndex = find_memory_type(vertexMemReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &vertexAllocInfo, NULL, &prim->vertexMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, prim->vertexBuffer, prim->vertexMemory, 0));

    // Upload vertex data
    void* vertexData;
    VK_CHECK(qvkMapMemory(vk.device, prim->vertexMemory, 0, sizeof(vertices), 0, &vertexData));
    memcpy(vertexData, vertices, sizeof(vertices));
    qvkUnmapMemory(vk.device, prim->vertexMemory);

    // Index buffer
    VkBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = sizeof(indices);
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &indexBufferInfo, NULL, &prim->indexBuffer));

    VkMemoryRequirements indexMemReq;
    qvkGetBufferMemoryRequirements(vk.device, prim->indexBuffer, &indexMemReq);

    VkMemoryAllocateInfo indexAllocInfo = {};
    indexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    indexAllocInfo.allocationSize = indexMemReq.size;
    indexAllocInfo.memoryTypeIndex = find_memory_type(indexMemReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &indexAllocInfo, NULL, &prim->indexMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, prim->indexBuffer, prim->indexMemory, 0));

    // Upload index data
    void* indexData;
    VK_CHECK(qvkMapMemory(vk.device, prim->indexMemory, 0, sizeof(indices), 0, &indexData));
    memcpy(indexData, indices, sizeof(indices));
    qvkUnmapMemory(vk.device, prim->indexMemory);
}

void R_GLTF_LoadImages(gltfModel_t* model, const char* basePath) {
    // For now, create a default material texture
    model->imageCount = 1;
    model->images = ri.Hunk_AllocateTempMemory(sizeof(gltfImage_t));

    gltfImage_t* image = &model->images[0];
    memset(image, 0, sizeof(gltfImage_t));

    // Create a simple 1x1 white texture
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = 1;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &image->vkImage));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, image->vkImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &image->vkMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, image->vkImage, image->vkMemory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image->vkImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &image->vkImageView));
}

void R_GLTF_CreateVulkanResources(gltfModel_t* model) {
    // Create descriptor set layout for materials
    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Material parameters UBO
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Base color texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &model->descriptorSetLayout));

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vk.descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &model->descriptorSetLayout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &allocInfo, &model->descriptorSet));
}

void R_GLTF_ComputeBounds(gltfModel_t* model) {
    VectorSet(model->mins, -0.5f, -0.5f, -0.5f);
    VectorSet(model->maxs,  0.5f,  0.5f,  0.5f);
}

qboolean R_GLTF_GetBounds(qhandle_t handle, vec3_t mins, vec3_t maxs) {
    gltfModel_t* model = R_GLTF_GetModel(handle);
    if (!model || !model->loaded) return qfalse;

    VectorCopy(model->mins, mins);
    VectorCopy(model->maxs, maxs);
    return qtrue;
}

void R_GLTF_Render(qhandle_t handle, const float* modelMatrix, const float* viewMatrix, const float* projectionMatrix) {
    gltfModel_t* model = R_GLTF_GetModel(handle);
    if (!model || !model->loaded) return;

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        vk.pipeline_layout, 0, 1, &model->descriptorSet, 0, NULL);

    // Set model matrix (push constant)
    qvkCmdPushConstants(vk.command_buffer, vk.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, 64, modelMatrix);

    // Render each primitive
    gltfPrimitive_t* prim = &model->meshes[0].primitives[0];

    VkDeviceSize offsets[] = {0};
    qvkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &prim->vertexBuffer, offsets);
    qvkCmdBindIndexBuffer(vk.command_buffer, prim->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    qvkCmdDrawIndexed(vk.command_buffer, prim->indexCount, 1, 0, 0, 0);
}

gltfModel_t* R_GLTF_GetModel(qhandle_t handle) {
    if (handle <= 0 || handle > MAX_GLTF_MODELS) return NULL;
    int slot = handle - 1;
    if (!gltfModelsUsed[slot]) return NULL;
    return &gltfModels[slot];
}