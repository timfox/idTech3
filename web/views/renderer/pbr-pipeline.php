<?php
/**
 * PBR Pipeline Implementation - JKSunny's PBR Port
 */
$title = 'PBR Pipeline Implementation - id Tech 3 Documentation';
$breadcrumbs = [
    '/renderer' => 'Renderer Deep Dive',
    '/renderer/pbr-pipeline' => 'PBR Pipeline'
];
?>

<h1>PBR Pipeline Implementation</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The PBR (Physically Based Rendering) pipeline in JKSunny's PBR port transforms id Tech 3's traditional fixed-function rendering into a modern, physically accurate system. This documentation covers the actual implementation as it exists in the Vulkan renderer.</p>
    
    <div class="feature-list">
        <h3>PBR Pipeline Features</h3>
        <ul>
            <li><strong>Material System:</strong> Albedo, normal, metallic-roughness, occlusion maps</li>
            <li><strong>Descriptor Management:</strong> Efficient resource binding for PBR assets</li>
            <li><strong>Unified Shaders:</strong> Single shader pipeline handling all PBR materials</li>
            <li><strong>Lighting Model:</strong> Cook-Torrance BRDF with IBL support</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>PBR Material System</h2>
    
    <h3>Material Structure</h3>
    <div class="code-block">
        <pre><code>// tr_pbr.h - PBR material definition
typedef struct pbrMaterial_s {
    // Base material properties
    vec3_t baseColor;           // RGB albedo color
    float metallicFactor;       // 0.0 = dielectric, 1.0 = metallic
    float roughnessFactor;      // 0.0 = mirror, 1.0 = completely rough
    vec3_t emissiveFactor;      // Self-illumination color
    float normalScale;          // Normal map intensity
    float occlusionStrength;    // Ambient occlusion strength
    
    // Texture handles (Vulkan image views)
    VkImageView baseColorTexture;
    VkImageView normalTexture;
    VkImageView metallicRoughnessTexture;  // Metallic (B) + Roughness (G)
    VkImageView occlusionTexture;          // Ambient occlusion (R)
    VkImageView emissiveTexture;
    
    // Sampling configuration
    VkSampler sampler;
    
    // Shader pipeline state
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    
    // Resource binding
    VkDescriptorSet descriptorSet;
    
    // Material flags
    int flags;
    #define MATERIAL_FLAG_ALPHA_BLEND    (1 << 0)
    #define MATERIAL_FLAG_ALPHA_TEST     (1 << 1)
    #define MATERIAL_FLAG_DOUBLE_SIDED   (1 << 2)
    #define MATERIAL_FLAG_EMISSIVE       (1 << 3)
    
} pbrMaterial_t;

// Global PBR material registry
#define MAX_PBR_MATERIALS 2048
static pbrMaterial_t pbrMaterials[MAX_PBR_MATERIALS];
static int numPBRMaterials = 0;

// Material creation and management
pbrMaterial_t* PBR_CreateMaterial(const char* name) {
    if (numPBRMaterials >= MAX_PBR_MATERIALS) {
        Com_Printf("^1PBR_CreateMaterial: MAX_PBR_MATERIALS exceeded\n");
        return NULL;
    }
    
    pbrMaterial_t* material = &pbrMaterials[numPBRMaterials++];
    memset(material, 0, sizeof(pbrMaterial_t));
    
    // Default PBR values
    VectorSet(material->baseColor, 1.0f, 1.0f, 1.0f);
    material->metallicFactor = 0.0f;
    material->roughnessFactor = 1.0f;
    VectorClear(material->emissiveFactor);
    material->normalScale = 1.0f;
    material->occlusionStrength = 1.0f;
    
    // Create default sampler
    if (!PBR_CreateMaterialSampler(material)) {
        numPBRMaterials--;
        return NULL;
    }
    
    return material;
}

qboolean PBR_CreateMaterialSampler(pbrMaterial_t* material) {
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    
    VkResult result = vkCreateSampler(vk.device, &samplerInfo, NULL, &material->sampler);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create material sampler: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
    
    <h3>Material Loading and Parsing</h3>
    <div class="code-block">
        <pre><code>// Material file parsing (.mtl format or custom JSON)
qboolean PBR_LoadMaterial(const char* materialPath, pbrMaterial_t* material) {
    char* materialData;
    int materialSize;
    
    materialSize = FS_ReadFile(materialPath, (void**)&materialData);
    if (!materialData) {
        Com_Printf("^3Warning: Could not load material file %s\n", materialPath);
        return qfalse;
    }
    
    // Parse material properties
    char* token;
    char* p = materialData;
    
    while ((token = COM_ParseExt(&p, qtrue)) && *token) {
        if (!Q_stricmp(token, "baseColor")) {
            material->baseColor[0] = atof(COM_ParseExt(&p, qfalse));
            material->baseColor[1] = atof(COM_ParseExt(&p, qfalse));
            material->baseColor[2] = atof(COM_ParseExt(&p, qfalse));
        }
        else if (!Q_stricmp(token, "metallicFactor")) {
            material->metallicFactor = atof(COM_ParseExt(&p, qfalse));
        }
        else if (!Q_stricmp(token, "roughnessFactor")) {
            material->roughnessFactor = atof(COM_ParseExt(&p, qfalse));
        }
        else if (!Q_stricmp(token, "emissiveFactor")) {
            material->emissiveFactor[0] = atof(COM_ParseExt(&p, qfalse));
            material->emissiveFactor[1] = atof(COM_ParseExt(&p, qfalse));
            material->emissiveFactor[2] = atof(COM_ParseExt(&p, qfalse));
        }
        else if (!Q_stricmp(token, "baseColorTexture")) {
            char texturePath[MAX_QPATH];
            Q_strncpyz(texturePath, COM_ParseExt(&p, qfalse), sizeof(texturePath));
            material->baseColorTexture = R_LoadTextureVK(texturePath, IMGTYPE_COLORALPHA, IMGFLAG_NONE);
        }
        else if (!Q_stricmp(token, "normalTexture")) {
            char texturePath[MAX_QPATH];
            Q_strncpyz(texturePath, COM_ParseExt(&p, qfalse), sizeof(texturePath));
            material->normalTexture = R_LoadTextureVK(texturePath, IMGTYPE_NORMAL, IMGFLAG_NONE);
        }
        else if (!Q_stricmp(token, "metallicRoughnessTexture")) {
            char texturePath[MAX_QPATH];
            Q_strncpyz(texturePath, COM_ParseExt(&p, qfalse), sizeof(texturePath));
            material->metallicRoughnessTexture = R_LoadTextureVK(texturePath, IMGTYPE_COLORALPHA, IMGFLAG_NONE);
        }
        else if (!Q_stricmp(token, "occlusionTexture")) {
            char texturePath[MAX_QPATH];
            Q_strncpyz(texturePath, COM_ParseExt(&p, qfalse), sizeof(texturePath));
            material->occlusionTexture = R_LoadTextureVK(texturePath, IMGTYPE_COLORALPHA, IMGFLAG_NONE);
        }
        else if (!Q_stricmp(token, "alphaMode")) {
            char* mode = COM_ParseExt(&p, qfalse);
            if (!Q_stricmp(mode, "BLEND")) {
                material->flags |= MATERIAL_FLAG_ALPHA_BLEND;
            } else if (!Q_stricmp(mode, "MASK")) {
                material->flags |= MATERIAL_FLAG_ALPHA_TEST;
            }
        }
        else if (!Q_stricmp(token, "doubleSided")) {
            if (atoi(COM_ParseExt(&p, qfalse))) {
                material->flags |= MATERIAL_FLAG_DOUBLE_SIDED;
            }
        }
    }
    
    FS_FreeFile(materialData);
    
    // Create descriptor set for this material
    if (!PBR_CreateMaterialDescriptorSet(material)) {
        Com_Printf("^1Failed to create descriptor set for material\n");
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Descriptor Set Layout</h2>
    
    <h3>PBR Resource Binding Layout</h3>
    <div class="code-block">
        <pre><code>// Descriptor set layout for PBR pipeline
typedef struct pbrDescriptors_s {
    VkDescriptorSetLayout materialLayout;    // Material textures and properties
    VkDescriptorSetLayout uniformLayout;     // Camera and lighting uniforms
    VkDescriptorSetLayout iblLayout;         // Image-based lighting
    
    VkDescriptorPool descriptorPool;
    
} pbrDescriptors_t;

static pbrDescriptors_t pbrDesc;

qboolean PBR_CreateDescriptorSetLayouts(void) {
    // Material descriptor set layout (set = 0)
    VkDescriptorSetLayoutBinding materialBindings[] = {
        // Textures
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        }, // baseColorTexture
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        }, // normalTexture
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        }, // metallicRoughnessTexture
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        }, // occlusionTexture
        {
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        }, // emissiveTexture
        // Material properties uniform buffer
        {
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    
    VkDescriptorSetLayoutCreateInfo materialLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(materialBindings),
        .pBindings = materialBindings,
    };
    
    VkResult result = vkCreateDescriptorSetLayout(vk.device, &materialLayoutInfo, 
                                                 NULL, &pbrDesc.materialLayout);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create material descriptor set layout: %s\n", 
                  VK_ResultToString(result));
        return qfalse;
    }
    
    // Uniform descriptor set layout (set = 1)
    VkDescriptorSetLayoutBinding uniformBindings[] = {
        // Camera uniform buffer
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        // Lighting uniform buffer
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    
    VkDescriptorSetLayoutCreateInfo uniformLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(uniformBindings),
        .pBindings = uniformBindings,
    };
    
    result = vkCreateDescriptorSetLayout(vk.device, &uniformLayoutInfo, 
                                        NULL, &pbrDesc.uniformLayout);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create uniform descriptor set layout: %s\n", 
                  VK_ResultToString(result));
        return qfalse;
    }
    
    // IBL descriptor set layout (set = 2)
    VkDescriptorSetLayoutBinding iblBindings[] = {
        // Environment cubemap
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        // Irradiance cubemap
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        // Prefiltered environment map
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
        // BRDF LUT
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    
    VkDescriptorSetLayoutCreateInfo iblLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(iblBindings),
        .pBindings = iblBindings,
    };
    
    result = vkCreateDescriptorSetLayout(vk.device, &iblLayoutInfo, 
                                        NULL, &pbrDesc.iblLayout);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create IBL descriptor set layout: %s\n", 
                  VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}

qboolean PBR_CreateDescriptorPool(void) {
    // Calculate pool sizes based on maximum materials and frames
    VkDescriptorPoolSize poolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_PBR_MATERIALS * 10,  // Material props + camera + lighting
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_PBR_MATERIALS * 20,  // 5 textures per material + IBL
        },
    };
    
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = MAX_PBR_MATERIALS * 3,  // material + uniform + ibl sets
    };
    
    VkResult result = vkCreateDescriptorPool(vk.device, &poolInfo, NULL, 
                                           &pbrDesc.descriptorPool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create PBR descriptor pool: %s\n", 
                  VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>PBR Uniform Buffers</h2>
    
    <h3>Uniform Buffer Structures</h3>
    <div class="code-block">
        <pre><code>// Uniform buffer objects for PBR pipeline
typedef struct pbrCameraUBO_s {
    matrix_t viewMatrix;
    matrix_t projectionMatrix;
    matrix_t viewProjectionMatrix;
    vec3_t viewPosition;
    float _padding1;
    vec3_t viewDirection;
    float _padding2;
} pbrCameraUBO_t;

typedef struct pbrLightingUBO_s {
    // Directional light (sun)
    vec3_t directionalLightDirection;
    float _padding1;
    vec3_t directionalLightColor;
    float directionalLightIntensity;
    
    // Point lights
    struct {
        vec3_t position;
        float range;
        vec3_t color;
        float intensity;
    } pointLights[MAX_POINT_LIGHTS];
    int numPointLights;
    
    // Environment lighting
    float iblIntensity;
    float ambientIntensity;
    float _padding2;
    
} pbrLightingUBO_t;

typedef struct pbrMaterialUBO_s {
    vec3_t baseColor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float _padding1;
    vec3_t emissiveFactor;
    float _padding2;
} pbrMaterialUBO_t;

// Global uniform buffer management
typedef struct pbrUniforms_s {
    // Buffers
    VkBuffer cameraBuffer;
    VkDeviceMemory cameraBufferMemory;
    void* cameraBufferMapped;
    
    VkBuffer lightingBuffer;
    VkDeviceMemory lightingBufferMemory;
    void* lightingBufferMapped;
    
    VkBuffer materialBuffers[MAX_PBR_MATERIALS];
    VkDeviceMemory materialBufferMemory[MAX_PBR_MATERIALS];
    void* materialBufferMapped[MAX_PBR_MATERIALS];
    
    // Descriptor sets
    VkDescriptorSet cameraDescriptorSet;
    VkDescriptorSet lightingDescriptorSet;
    
} pbrUniforms_t;

static pbrUniforms_t pbrUniforms;

qboolean PBR_CreateUniformBuffers(void) {
    VkDeviceSize cameraBufferSize = sizeof(pbrCameraUBO_t);
    VkDeviceSize lightingBufferSize = sizeof(pbrLightingUBO_t);
    VkDeviceSize materialBufferSize = sizeof(pbrMaterialUBO_t);
    
    // Create camera uniform buffer
    if (!VK_CreateBuffer(cameraBufferSize, 
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &pbrUniforms.cameraBuffer, 
                        &pbrUniforms.cameraBufferMemory)) {
        Com_Printf("^1Failed to create camera uniform buffer\n");
        return qfalse;
    }
    
    // Map camera buffer
    vkMapMemory(vk.device, pbrUniforms.cameraBufferMemory, 0, cameraBufferSize, 0, 
               &pbrUniforms.cameraBufferMapped);
    
    // Create lighting uniform buffer
    if (!VK_CreateBuffer(lightingBufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &pbrUniforms.lightingBuffer,
                        &pbrUniforms.lightingBufferMemory)) {
        Com_Printf("^1Failed to create lighting uniform buffer\n");
        return qfalse;
    }
    
    // Map lighting buffer
    vkMapMemory(vk.device, pbrUniforms.lightingBufferMemory, 0, lightingBufferSize, 0,
               &pbrUniforms.lightingBufferMapped);
    
    // Create material uniform buffers (one per material)
    for (int i = 0; i < MAX_PBR_MATERIALS; i++) {
        if (!VK_CreateBuffer(materialBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &pbrUniforms.materialBuffers[i],
                            &pbrUniforms.materialBufferMemory[i])) {
            Com_Printf("^1Failed to create material uniform buffer %d\n", i);
            return qfalse;
        }
        
        // Map material buffer
        vkMapMemory(vk.device, pbrUniforms.materialBufferMemory[i], 0, materialBufferSize, 0,
                   &pbrUniforms.materialBufferMapped[i]);
    }
    
    return qtrue;
}

void PBR_UpdateCameraUniforms(const refdef_t* refdef) {
    pbrCameraUBO_t cameraUBO;
    
    // Copy matrices
    Matrix_Copy(refdef->viewMatrix, cameraUBO.viewMatrix);
    Matrix_Copy(refdef->projectionMatrix, cameraUBO.projectionMatrix);
    Matrix_Multiply(refdef->projectionMatrix, refdef->viewMatrix, cameraUBO.viewProjectionMatrix);
    
    // Copy view position and direction
    VectorCopy(refdef->vieworg, cameraUBO.viewPosition);
    AngleVectors(refdef->viewangles, cameraUBO.viewDirection, NULL, NULL);
    
    // Update buffer
    memcpy(pbrUniforms.cameraBufferMapped, &cameraUBO, sizeof(cameraUBO));
}

void PBR_UpdateLightingUniforms(void) {
    pbrLightingUBO_t lightingUBO = {0};
    
    // Update directional light (sun)
    if (r_sun->integer) {
        VectorCopy(tr.sunDirection, lightingUBO.directionalLightDirection);
        VectorCopy(tr.sunLight, lightingUBO.directionalLightColor);
        lightingUBO.directionalLightIntensity = tr.sunIntensity;
    }
    
    // Update point lights
    lightingUBO.numPointLights = min(tr.numDlights, MAX_POINT_LIGHTS);
    for (int i = 0; i < lightingUBO.numPointLights; i++) {
        dlight_t* dl = &tr.dlights[i];
        VectorCopy(dl->origin, lightingUBO.pointLights[i].position);
        VectorCopy(dl->color, lightingUBO.pointLights[i].color);
        lightingUBO.pointLights[i].intensity = dl->intensity;
        lightingUBO.pointLights[i].range = dl->radius;
    }
    
    // Environment lighting settings
    lightingUBO.iblIntensity = r_iblIntensity->value;
    lightingUBO.ambientIntensity = r_ambientScale->value;
    
    // Update buffer
    memcpy(pbrUniforms.lightingBufferMapped, &lightingUBO, sizeof(lightingUBO));
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>PBR Shader Pipeline</h2>
    
    <h3>Pipeline State Object Creation</h3>
    <div class="code-block">
        <pre><code>// PBR graphics pipeline creation
qboolean PBR_CreateGraphicsPipeline(void) {
    // Load PBR shaders
    VkShaderModule vertShaderModule = VK_LoadShaderModule("shaders/pbr.vert.spv");
    VkShaderModule fragShaderModule = VK_LoadShaderModule("shaders/pbr.frag.spv");
    
    if (!vertShaderModule || !fragShaderModule) {
        Com_Printf("^1Failed to load PBR shaders\n");
        return qfalse;
    }
    
    // Shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main",
    };
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main",
    };
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo
    };
    
    // Vertex input description
    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(drawVert_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    
    VkVertexInputAttributeDescription attributeDescriptions[] = {
        // Position
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(drawVert_t, xyz),
        },
        // Normal
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(drawVert_t, normal),
        },
        // Texture coordinates
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(drawVert_t, st),
        },
        // Tangent
        {
            .binding = 0,
            .location = 3,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(drawVert_t, tangent),
        },
        // Color
        {
            .binding = 0,
            .location = 4,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = offsetof(drawVert_t, color),
        },
    };
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = ARRAY_LEN(attributeDescriptions),
        .pVertexAttributeDescriptions = attributeDescriptions,
    };
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    
    // Viewport and scissor
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)vk.swapchainExtent.width,
        .height = (float)vk.swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = vk.swapchainExtent,
    };
    
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    
    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    
    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };
    
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f,
    };
    
    // Pipeline layout
    VkDescriptorSetLayout setLayouts[] = {
        pbrDesc.materialLayout,
        pbrDesc.uniformLayout,
        pbrDesc.iblLayout
    };
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = ARRAY_LEN(setLayouts),
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 0,
    };
    
    VkResult result = vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, 
                                           &pbrPipelineLayout);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create PBR pipeline layout: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .layout = pbrPipelineLayout,
        .renderPass = vk.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };
    
    result = vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, 
                                      NULL, &pbrPipeline);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create PBR graphics pipeline: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Cleanup shader modules
    vkDestroyShaderModule(vk.device, fragShaderModule, NULL);
    vkDestroyShaderModule(vk.device, vertShaderModule, NULL);
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>PBR Rendering Loop</h2>
    
    <h3>Material Binding and Draw Calls</h3>
    <div class="code-block">
        <pre><code>// PBR rendering integration with main render loop
void PBR_BeginFrame(void) {
    // Update uniform buffers for this frame
    PBR_UpdateCameraUniforms(&tr.refdef);
    PBR_UpdateLightingUniforms();
    
    // Bind PBR pipeline
    vkCmdBindPipeline(vk.commandBuffers[vk.currentFrame], 
                     VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);
    
    // Bind global descriptor sets (camera + lighting + IBL)
    VkDescriptorSet globalSets[] = {
        pbrUniforms.cameraDescriptorSet,
        pbrUniforms.lightingDescriptorSet,
        pbrIBL.descriptorSet
    };
    
    vkCmdBindDescriptorSets(vk.commandBuffers[vk.currentFrame],
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pbrPipelineLayout,
                           1, 3, globalSets,
                           0, NULL);
}

void PBR_DrawSurface(msurface_t* surface, pbrMaterial_t* material) {
    VkCommandBuffer cmd = vk.commandBuffers[vk.currentFrame];
    
    // Update material uniform buffer
    pbrMaterialUBO_t materialUBO;
    VectorCopy(material->baseColor, materialUBO.baseColor);
    materialUBO.metallicFactor = material->metallicFactor;
    materialUBO.roughnessFactor = material->roughnessFactor;
    materialUBO.normalScale = material->normalScale;
    materialUBO.occlusionStrength = material->occlusionStrength;
    VectorCopy(material->emissiveFactor, materialUBO.emissiveFactor);
    
    // Find material buffer index
    int materialIndex = material - pbrMaterials;
    memcpy(pbrUniforms.materialBufferMapped[materialIndex], &materialUBO, sizeof(materialUBO));
    
    // Bind material descriptor set (textures + material properties)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pbrPipelineLayout, 0, 1, &material->descriptorSet,
                           0, NULL);
    
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {surface->vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, surface->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw indexed
    vkCmdDrawIndexed(cmd, surface->numIndexes, 1, 0, 0, 0);
}

void PBR_RenderWorld(void) {
    // Render opaque surfaces first
    for (int i = 0; i < tr.world->numSurfaces; i++) {
        msurface_t* surface = &tr.world->surfaces[i];
        
        if (surface->shader->contentFlags & CONTENTS_TRANSLUCENT) {
            continue; // Skip transparent surfaces for now
        }
        
        // Get PBR material for this surface
        pbrMaterial_t* material = PBR_GetMaterialForShader(surface->shader);
        if (!material) {
            material = &pbrDefaultMaterial; // Fallback
        }
        
        PBR_DrawSurface(surface, material);
    }
    
    // TODO: Render transparent surfaces in back-to-front order
}

void PBR_RenderEntities(void) {
    for (int i = 0; i < tr.refdef.num_entities; i++) {
        refEntity_t* ent = &tr.refdef.entities[i];
        
        if (ent->e.reType != RT_MODEL) {
            continue;
        }
        
        model_t* model = R_GetModelByHandle(ent->e.hModel);
        if (!model || model->type != MOD_MESH) {
            continue;
        }
        
        // Set up entity transform (push constants or uniform buffer)
        PBR_SetEntityTransform(ent);
        
        // Render all surfaces of the model
        for (int j = 0; j < model->numSurfaces; j++) {
            msurface_t* surface = &model->surfaces[j];
            pbrMaterial_t* material = PBR_GetMaterialForShader(surface->shader);
            
            if (!material) {
                material = &pbrDefaultMaterial;
            }
            
            PBR_DrawSurface(surface, material);
        }
    }
}

// Main PBR render function called from tr_backend.c
void PBR_RenderFrame(void) {
    PBR_BeginFrame();
    
    // Render world geometry
    PBR_RenderWorld();
    
    // Render entities
    PBR_RenderEntities();
    
    // TODO: Render transparent objects
    // TODO: Post-processing pass
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>IBL Integration</h2>
    
    <h3>Image-Based Lighting Setup</h3>
    <div class="code-block">
        <pre><code>// Image-based lighting for realistic environment reflections
typedef struct pbrIBL_s {
    // Environment maps
    VkImage environmentMap;      // HDR environment cubemap
    VkImageView environmentMapView;
    
    VkImage irradianceMap;       // Diffuse irradiance cubemap
    VkImageView irradianceMapView;
    
    VkImage prefilteredMap;      // Specular prefiltered environment map
    VkImageView prefilteredMapView;
    
    VkImage brdfLUT;             // BRDF integration lookup table
    VkImageView brdfLUTView;
    
    // Sampling
    VkSampler cubemapSampler;
    VkSampler brdfSampler;
    
    // Descriptor set
    VkDescriptorSet descriptorSet;
    
} pbrIBL_t;

static pbrIBL_t pbrIBL;

qboolean PBR_InitializeIBL(const char* environmentPath) {
    Com_Printf("Initializing PBR IBL with %s\n", environmentPath);
    
    // Load HDR environment map
    if (!PBR_LoadEnvironmentMap(environmentPath)) {
        Com_Printf("^1Failed to load environment map\n");
        return qfalse;
    }
    
    // Generate irradiance map for diffuse lighting
    if (!PBR_GenerateIrradianceMap()) {
        Com_Printf("^1Failed to generate irradiance map\n");
        return qfalse;
    }
    
    // Generate prefiltered environment map for specular reflections
    if (!PBR_GeneratePrefilteredMap()) {
        Com_Printf("^1Failed to generate prefiltered map\n");
        return qfalse;
    }
    
    // Generate BRDF lookup table
    if (!PBR_GenerateBRDFLUT()) {
        Com_Printf("^1Failed to generate BRDF LUT\n");
        return qfalse;
    }
    
    // Create samplers
    if (!PBR_CreateIBLSamplers()) {
        Com_Printf("^1Failed to create IBL samplers\n");
        return qfalse;
    }
    
    // Create descriptor set
    if (!PBR_CreateIBLDescriptorSet()) {
        Com_Printf("^1Failed to create IBL descriptor set\n");
        return qfalse;
    }
    
    return qtrue;
}

qboolean PBR_LoadEnvironmentMap(const char* path) {
    // Load HDR image (typically .hdr or .exr format)
    int width, height, channels;
    float* hdrData = stbi_loadf(path, &width, &height, &channels, 4);
    
    if (!hdrData) {
        Com_Printf("^1Failed to load HDR environment map: %s\n", path);
        return qfalse;
    }
    
    // Convert equirectangular to cubemap
    int cubemapSize = 512;  // Resolution per face
    if (!PBR_EquirectangularToCubemap(hdrData, width, height, cubemapSize)) {
        stbi_image_free(hdrData);
        return qfalse;
    }
    
    stbi_image_free(hdrData);
    return qtrue;
}

qboolean PBR_GenerateIrradianceMap(void) {
    // Convolve environment map for diffuse irradiance
    // This is typically done as a compute shader or render-to-texture pass
    
    int irradianceSize = 32;  // Small resolution for diffuse convolution
    
    // Create irradiance cubemap
    if (!VK_CreateCubemapImage(irradianceSize, VK_FORMAT_R16G16B16A16_SFLOAT,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              &pbrIBL.irradianceMap, &pbrIBL.irradianceMapView)) {
        return qfalse;
    }
    
    // Render convolution (simplified - normally done with compute shader)
    // For each face of the cubemap, convolve the environment map
    // This involves importance sampling the hemisphere
    
    return qtrue;
}

qboolean PBR_GeneratePrefilteredMap(void) {
    // Generate prefiltered environment map for specular reflections
    // Multiple mip levels represent different roughness values
    
    int prefilteredSize = 128;
    int mipLevels = 5;  // Different roughness levels
    
    // Create prefiltered cubemap with mipmaps
    if (!VK_CreateCubemapImageWithMips(prefilteredSize, mipLevels, 
                                      VK_FORMAT_R16G16B16A16_SFLOAT,
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      &pbrIBL.prefilteredMap, &pbrIBL.prefilteredMapView)) {
        return qfalse;
    }
    
    // For each mip level, render with different roughness
    // Mip 0 = roughness 0.0 (mirror reflection)
    // Mip 4 = roughness 1.0 (completely rough)
    
    return qtrue;
}

qboolean PBR_GenerateBRDFLUT(void) {
    // Generate BRDF integration lookup table
    // This is a 2D texture where:
    // X axis = NdotV (normal dot view)
    // Y axis = roughness
    // Result = (scale, bias) for F0 term
    
    int lutSize = 512;
    
    if (!VK_CreateImage2D(lutSize, lutSize, VK_FORMAT_R16G16_SFLOAT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         &pbrIBL.brdfLUT, &pbrIBL.brdfLUTView)) {
        return qfalse;
    }
    
    // Render BRDF LUT using a fullscreen quad and compute shader
    // This is done once at startup
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/renderer/vulkan-implementation">Vulkan Renderer</a></li>
        <li><a href="/renderer/resource-management">Resource Management</a></li>
        <li><a href="/rendering/shaders">Shaders</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
        <li><a href="/modernization/profiling-tools">Performance Profiling</a></li>
    </ul>
</div>