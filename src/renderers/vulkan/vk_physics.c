#include "vk_physics.h"
#include "vk.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../common/qcommon.h"
#include <string.h>
#include <math.h>

vk_physics_t vk_physics;

void VK_Physics_Init(void) {
    memset(&vk_physics, 0, sizeof(vk_physics_t));

    // Check if compute queue is available
    // For now, we'll use the graphics queue for compute
    // In a full implementation, we'd check for a dedicated compute queue

    // Create compute command pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vk.queue_family_index;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(qvkCreateCommandPool(vk.device, &poolInfo, NULL, &vk_physics.computeCommandPool));

    // Allocate compute command buffer
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vk_physics.computeCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(qvkAllocateCommandBuffers(vk.device, &allocInfo, &vk_physics.computeCommandBuffer));

    // Create fence for compute synchronization
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(qvkCreateFence(vk.device, &fenceInfo, NULL, &vk_physics.computeFence));

    // Create semaphores for compute/graphics synchronization
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(qvkCreateSemaphore(vk.device, &semaphoreInfo, NULL, &vk_physics.computeReadySemaphore));
    VK_CHECK(qvkCreateSemaphore(vk.device, &semaphoreInfo, NULL, &vk_physics.computeCompleteSemaphore));

    // Create descriptor set layout for compute shader
    VkDescriptorSetLayoutBinding bindings[3] = {};

    // Storage buffer input
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Storage buffer output
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Uniform buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk_physics.computeDescriptorSetLayout));

    // Create pipeline layout
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t);  // For calculateNormals flag

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk_physics.computeDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk_physics.computePipelineLayout));

    // Create uniform buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(float) * 32 + sizeof(int) * 2;  // Parameters + particle count
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &bufferInfo, NULL, &vk_physics.uniformBuffer));

    VkMemoryRequirements memReq;
    qvkGetBufferMemoryRequirements(vk.device, vk_physics.uniformBuffer, &memReq);

    VkMemoryAllocateInfo allocMemInfo = {};
    allocMemInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocMemInfo.allocationSize = memReq.size;
    allocMemInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocMemInfo, NULL, &vk_physics.uniformBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_physics.uniformBuffer, vk_physics.uniformBufferMemory, 0));

    VK_CHECK(qvkMapMemory(vk.device, vk_physics.uniformBufferMemory, 0, bufferInfo.size, 0, &vk_physics.uniformBufferMapped));

    // Default settings
    VectorSet(vk_physics.gravity, 0.0f, -9.8f, 0.0f);
    VectorSet(vk_physics.spherePos, 0.0f, 0.0f, 0.0f);
    vk_physics.sphereRadius = 1.0f;
    vk_physics.simulateWind = qfalse;
    vk_physics.calculateNormals = qtrue;
    vk_physics.simulationIterations = 64;
    vk_physics.currentBufferIndex = 0;
    vk_physics.enabled = qtrue;
    vk_physics.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Physics simulation system initialized\n");
}

void VK_Physics_Shutdown(void) {
    if (!vk_physics.initialized) return;

    // Unmap uniform buffer
    if (vk_physics.uniformBufferMapped) {
        qvkUnmapMemory(vk.device, vk_physics.uniformBufferMemory);
    }

    // Destroy uniform buffer
    if (vk_physics.uniformBuffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, vk_physics.uniformBuffer, NULL);
    }
    if (vk_physics.uniformBufferMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_physics.uniformBufferMemory, NULL);
    }

    // Destroy storage buffers
    for (int i = 0; i < 2; i++) {
        if (vk_physics.storageBufferMapped[i]) {
            qvkUnmapMemory(vk.device, vk_physics.storageBufferMemory[i]);
        }
        if (vk_physics.storageBuffers[i] != VK_NULL_HANDLE) {
            qvkDestroyBuffer(vk.device, vk_physics.storageBuffers[i], NULL);
        }
        if (vk_physics.storageBufferMemory[i] != VK_NULL_HANDLE) {
            qvkFreeMemory(vk.device, vk_physics.storageBufferMemory[i], NULL);
        }
    }

    // Destroy compute pipeline
    if (vk_physics.computePipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk_physics.computePipeline, NULL);
    }
    if (vk_physics.computePipelineLayout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk_physics.computePipelineLayout, NULL);
    }
    if (vk_physics.computeDescriptorSetLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_physics.computeDescriptorSetLayout, NULL);
    }

    // Destroy synchronization primitives
    if (vk_physics.computeFence != VK_NULL_HANDLE) {
        qvkDestroyFence(vk.device, vk_physics.computeFence, NULL);
    }
    if (vk_physics.computeReadySemaphore != VK_NULL_HANDLE) {
        qvkDestroySemaphore(vk.device, vk_physics.computeReadySemaphore, NULL);
    }
    if (vk_physics.computeCompleteSemaphore != VK_NULL_HANDLE) {
        qvkDestroySemaphore(vk.device, vk_physics.computeCompleteSemaphore, NULL);
    }

    // Destroy command pool
    if (vk_physics.computeCommandPool != VK_NULL_HANDLE) {
        qvkDestroyCommandPool(vk.device, vk_physics.computeCommandPool, NULL);
    }

    memset(&vk_physics, 0, sizeof(vk_physics_t));
}

void VK_Physics_CreateCloth(int gridSizeX, int gridSizeY, float sizeX, float sizeY) {
    if (!vk_physics.initialized) return;

    vk_physics.clothParams.gridSizeX = gridSizeX;
    vk_physics.clothParams.gridSizeY = gridSizeY;
    vk_physics.clothParams.sizeX = sizeX;
    vk_physics.clothParams.sizeY = sizeY;
    vk_physics.clothParams.particleMass = 0.1f;
    vk_physics.clothParams.springStiffness = 2000.0f;
    vk_physics.clothParams.damping = 0.25f;

    // Calculate rest distances
    float dx = sizeX / (gridSizeX - 1);
    float dy = sizeY / (gridSizeY - 1);
    vk_physics.clothParams.restDistH = dx;
    vk_physics.clothParams.restDistV = dy;
    vk_physics.clothParams.restDistD = sqrtf(dx * dx + dy * dy);

    vk_physics.particleCount = gridSizeX * gridSizeY;
    int particleBufferSize = vk_physics.particleCount * sizeof(vk_physics_particle_t);

    // Create storage buffers (double-buffered)
    for (int i = 0; i < 2; i++) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = particleBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(qvkCreateBuffer(vk.device, &bufferInfo, NULL, &vk_physics.storageBuffers[i]));

        VkMemoryRequirements memReq;
        qvkGetBufferMemoryRequirements(vk.device, vk_physics.storageBuffers[i], &memReq);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_physics.storageBufferMemory[i]));
        VK_CHECK(qvkBindBufferMemory(vk.device, vk_physics.storageBuffers[i], vk_physics.storageBufferMemory[i], 0));

        // Initialize particle data
        vk_physics_particle_t* particles = (vk_physics_particle_t*)ri.Hunk_AllocateTempMemory(particleBufferSize);
        
        float du = 1.0f / (gridSizeX - 1);
        float dv = 1.0f / (gridSizeY - 1);

        // Create flat cloth grid
        for (int y = 0; y < gridSizeY; y++) {
            for (int x = 0; x < gridSizeX; x++) {
                int idx = y * gridSizeX + x;
                particles[idx].pos[0] = -sizeX / 2.0f + dx * x;
                particles[idx].pos[1] = -2.0f;
                particles[idx].pos[2] = -sizeY / 2.0f + dy * y;
                particles[idx].pos[3] = 1.0f;

                particles[idx].vel[0] = 0.0f;
                particles[idx].vel[1] = 0.0f;
                particles[idx].vel[2] = 0.0f;
                particles[idx].vel[3] = 0.0f;
                particles[idx].uv[0] = 1.0f - du * y;
                particles[idx].uv[1] = dv * x;
                particles[idx].uv[2] = 0.0f;
                particles[idx].uv[3] = 0.0f;
                particles[idx].normal[0] = 0.0f;
                particles[idx].normal[1] = 1.0f;
                particles[idx].normal[2] = 0.0f;
                particles[idx].normal[3] = 0.0f;
            }
        }

        // Upload to GPU (would use staging buffer in full implementation)
        // For now, this is a placeholder
        ri.Hunk_FreeTempMemory(particles);
    }

    ri.Printf(PRINT_ALL, "Created cloth simulation: %dx%d grid, %.2fx%.2f size\n",
        gridSizeX, gridSizeY, sizeX, sizeY);
}

void VK_Physics_Update(float deltaTime) {
    if (!vk_physics.enabled || !vk_physics.initialized) return;

    // Clamp delta time to prevent large jumps
    vk_physics.deltaTime = Com_Clamp(0.0f, 0.02f, deltaTime) * 0.0025f;

    // Update uniform buffer
    if (vk_physics.uniformBufferMapped) {
        float* uniforms = (float*)vk_physics.uniformBufferMapped;
        
        // Delta time
        uniforms[0] = vk_physics.deltaTime;
        
        // Cloth parameters
        uniforms[1] = vk_physics.clothParams.particleMass;
        uniforms[2] = vk_physics.clothParams.springStiffness;
        uniforms[3] = vk_physics.clothParams.damping;
        uniforms[4] = vk_physics.clothParams.restDistH;
        uniforms[5] = vk_physics.clothParams.restDistV;
        uniforms[6] = vk_physics.clothParams.restDistD;
        
        // Sphere collision
        uniforms[7] = vk_physics.sphereRadius;
        uniforms[8] = vk_physics.spherePos[0];
        uniforms[9] = vk_physics.spherePos[1];
        uniforms[10] = vk_physics.spherePos[2];
        
        // Gravity
        uniforms[11] = vk_physics.gravity[0];
        uniforms[12] = vk_physics.gravity[1];
        uniforms[13] = vk_physics.gravity[2];
        
        // Particle count
        int* particleCount = (int*)(uniforms + 14);
        particleCount[0] = vk_physics.clothParams.gridSizeX;
        particleCount[1] = vk_physics.clothParams.gridSizeY;
    }

    // Compute shader execution would happen here
    // This is a placeholder - full implementation would:
    // 1. Build compute command buffer
    // 2. Bind pipeline and descriptor sets
    // 3. Dispatch compute shader
    // 4. Synchronize with graphics queue
}

void VK_Physics_SetGravity(const vec3_t gravity) {
    VectorCopy(gravity, vk_physics.gravity);
}

void VK_Physics_SetCollisionSphere(const vec3_t position, float radius) {
    VectorCopy(position, vk_physics.spherePos);
    vk_physics.sphereRadius = radius;
}

void VK_Physics_SetWindEnabled(qboolean enabled) {
    vk_physics.simulateWind = enabled;
}

void VK_Physics_SetSimulationIterations(int iterations) {
    vk_physics.simulationIterations = Com_Clamp(1, 256, iterations);
}

VkBuffer VK_Physics_GetCurrentVertexBuffer(void) {
    if (!vk_physics.initialized) return VK_NULL_HANDLE;
    return vk_physics.storageBuffers[vk_physics.currentBufferIndex];
}

int VK_Physics_GetParticleCount(void) {
    return vk_physics.particleCount;
}

qboolean VK_Physics_IsEnabled(void) {
    return vk_physics.enabled && vk_physics.initialized;
}

void VK_Physics_SetEnabled(qboolean enabled) {
    vk_physics.enabled = enabled;
}
