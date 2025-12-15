#pragma once

#include "tr_local.h"
#include <stdint.h>

// glTF 2.0 loader for Vulkan renderer
// Supports PBR materials, animations, and scene hierarchies

// glTF data structures
typedef struct gltfAccessor {
    int bufferView;
    int byteOffset;
    int componentType; // GL_BYTE, GL_UNSIGNED_BYTE, etc.
    int count;
    char type[16]; // "SCALAR", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3", "MAT4"
    float min[16];
    float max[16];
} gltfAccessor_t;

typedef struct gltfBufferView {
    int buffer;
    int byteOffset;
    int byteLength;
    int byteStride;
    int target; // GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER
} gltfBufferView_t;

typedef struct gltfBuffer {
    int byteLength;
    char* uri;
    void* data;
    VkBuffer vkBuffer;
    VkDeviceMemory vkMemory;
} gltfBuffer_t;

typedef struct gltfImage {
    char* uri;
    char* mimeType;
    int bufferView;
    qhandle_t shaderHandle;
    VkImage vkImage;
    VkImageView vkImageView;
    VkDeviceMemory vkMemory;
} gltfImage_t;

typedef struct gltfTexture {
    int sampler;
    int source;
} gltfTexture_t;

typedef struct gltfMaterial {
    char name[64];
    vec4_t baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    vec3_t emissiveFactor;
    int baseColorTexture;
    int metallicRoughnessTexture;
    int normalTexture;
    int emissiveTexture;
    int occlusionTexture;
    qboolean doubleSided;
    int alphaMode; // 0=OPAQUE, 1=MASK, 2=BLEND
    float alphaCutoff;
} gltfMaterial_t;

typedef struct gltfPrimitive {
    int indices;
    int material;
    int mode; // GL_POINTS, GL_LINES, etc.
    struct {
        int position;
        int normal;
        int tangent;
        int texcoord[8];
        int color[8];
        int joints[8];
        int weights[8];
    } attributes;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexMemory;
    VkDeviceMemory indexMemory;
    int vertexCount;
    int indexCount;
} gltfPrimitive_t;

typedef struct gltfMesh {
    char name[64];
    gltfPrimitive_t* primitives;
    int primitiveCount;
    float* morphTargets;
    int morphTargetCount;
} gltfMesh_t;

typedef struct gltfNode {
    char name[64];
    int mesh;
    int skin;
    vec3_t translation;
    vec4_t rotation;
    vec3_t scale;
    int* children;
    int childCount;
    float* matrix; // 16 floats
    qboolean hasMatrix;
    // Computed transform
    float localMatrix[16];
    float worldMatrix[16];
} gltfNode_t;

typedef struct gltfScene {
    char name[64];
    int* nodes;
    int nodeCount;
} gltfScene_t;

typedef struct gltfModel {
    char filename[MAX_QPATH];
    qboolean loaded;

    // glTF JSON data
    gltfAccessor_t* accessors;
    gltfBufferView_t* bufferViews;
    gltfBuffer_t* buffers;
    gltfImage_t* images;
    gltfTexture_t* textures;
    gltfMaterial_t* materials;
    gltfMesh_t* meshes;
    gltfNode_t* nodes;
    gltfScene_t* scenes;

    int accessorCount;
    int bufferViewCount;
    int bufferCount;
    int imageCount;
    int textureCount;
    int materialCount;
    int meshCount;
    int nodeCount;
    int sceneCount;
    int defaultScene;

    // Vulkan resources
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Animation data (future)
    struct {
        // TODO: Animation support
    } animations;

    // Computed bounds
    vec3_t mins, maxs;
} gltfModel_t;

// glTF loading API
qhandle_t R_LoadGLTF(const char* filename);
void R_FreeGLTF(qhandle_t handle);

qboolean R_GLTF_GetBounds(qhandle_t handle, vec3_t mins, vec3_t maxs);
void R_GLTF_Render(qhandle_t handle, const float* modelMatrix, const float* viewMatrix, const float* projectionMatrix);

// Internal functions
gltfModel_t* R_GLTF_GetModel(qhandle_t handle);
void R_GLTF_ComputeNodeTransforms(gltfModel_t* model, gltfNode_t* node, const float* parentMatrix);
void R_GLTF_LoadBuffers(gltfModel_t* model, const char* jsonData);
void R_GLTF_LoadImages(gltfModel_t* model, const char* basePath);
void R_GLTF_CreateVulkanResources(gltfModel_t* model);