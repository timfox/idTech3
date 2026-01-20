#include "vk_buffers.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include "vk_memory.h"

// Renderer interface
extern refimport_t ri;

// Object naming macro
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

#ifdef USE_VMA
#include "vk_mem_alloc.h"

// VMA buffer allocation structures
VmaBufferAllocation vk_geometry_buffer_vma[NUM_COMMAND_BUFFERS];
VmaAllocation vk_geometry_buffer_memory_vma = VK_NULL_HANDLE;
VmaAllocation vk_storage_buffer_allocation = VK_NULL_HANDLE;
VmaAllocation vk_staging_buffer_allocation = VK_NULL_HANDLE;
VmaAllocation vk_vbo_allocation = VK_NULL_HANDLE;
#endif

// Forward declarations for functions used from vk.c
extern Vk_Instance vk;  // Main Vulkan instance
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern const char *vk_result_string(VkResult result);

// Vulkan function pointer extern declarations
extern PFN_vkCmdCopyBuffer qvkCmdCopyBuffer;

// Forward declarations for utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);

// Forward declaration for staging buffer flush
extern void vk_flush_staging_buffer(qboolean final);

void vk_release_geometry_buffers(void) {
    int i;

    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        qvkDestroyBuffer(vk.device, vk.tess[i].vertex_buffer, NULL);
        vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
#ifdef USE_VMA
        if (vk_geometry_buffer_vma[i].allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(vk.allocator, vk.tess[i].vertex_buffer, vk_geometry_buffer_vma[i].allocation);
            vk_geometry_buffer_vma[i].allocation = VK_NULL_HANDLE;
        }
#endif
    }

#ifndef USE_VMA
    qvkFreeMemory(vk.device, vk.geometry_buffer_memory, NULL);
    vk.geometry_buffer_memory = VK_NULL_HANDLE;
#else
    if (vk_geometry_buffer_memory_vma != VK_NULL_HANDLE) {
        vmaFreeMemory(vk.allocator, vk_geometry_buffer_memory_vma);
        vk_geometry_buffer_memory_vma = VK_NULL_HANDLE;
    }
#endif
}

void vk_create_geometry_buffers(VkDeviceSize size) {
    int i;

#ifdef USE_VMA
    // Use VMA for better memory management
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        // Mapped + AUTO requires a host access flag to satisfy VMA assertions.
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    VkMemoryRequirements vb_memory_requirements = {0};

    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        VkResult res = vmaCreateBuffer(vk.allocator, &bufferInfo, &allocCreateInfo,
            &vk.tess[i].vertex_buffer, &vk_geometry_buffer_vma[i].allocation, &vk_geometry_buffer_vma[i].allocationInfo);

        if (res != VK_SUCCESS) {
            ri.Error(ERR_FATAL, "VMA: Failed to create geometry buffer %i: %s", i, vk_result_string(res));
        }

        // Get memory requirements for size calculation
        qvkGetBufferMemoryRequirements(vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements);

        vk.tess[i].vertex_buffer_ptr = (byte*)vk_geometry_buffer_vma[i].allocationInfo.pMappedData;
        vk.tess[i].vertex_buffer_offset = 0;

        SET_OBJECT_NAME(vk.tess[i].vertex_buffer, va("geometry buffer %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
    }

    vk.geometry_buffer_size = vb_memory_requirements.size;
    vk.geometry_buffer_memory = VK_NULL_HANDLE; // Not used with VMA

#else
    // Original manual allocation code
    VkMemoryRequirements vb_memory_requirements;
    VkMemoryAllocateInfo alloc_info;
    VkBufferCreateInfo desc;
    VkDeviceSize vertex_buffer_offset;
    uint32_t memory_type_bits;
    uint32_t memory_type;
    void *data;

    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;

    Com_Memset(&vb_memory_requirements, 0, sizeof(vb_memory_requirements));

    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        desc.size = size;
        desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &vk.tess[i].vertex_buffer));

        qvkGetBufferMemoryRequirements(vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements);
    }

    memory_type_bits = vb_memory_requirements.memoryTypeBits;
    memory_type = find_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
    alloc_info.memoryTypeIndex = memory_type;

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory));
    VK_CHECK(qvkMapMemory(vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));

    vertex_buffer_offset = 0;

    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        qvkBindBufferMemory(vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset);
        vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
        vk.tess[i].vertex_buffer_offset = 0;
        vertex_buffer_offset += vb_memory_requirements.size;

        SET_OBJECT_NAME(vk.tess[i].vertex_buffer, va("geometry buffer %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
    }

    SET_OBJECT_NAME(vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

    vk.geometry_buffer_size = vb_memory_requirements.size;
#endif

    Com_Memset(&vk.stats, 0, sizeof(vk.stats));
}

void vk_create_storage_buffer(uint32_t size) {
#ifdef USE_VMA
    // Use VMA for better memory management
    VkBufferCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    VkResult res = vmaCreateBuffer(vk.allocator, &desc, &allocCreateInfo,
        &vk.storage.buffer, &vk_storage_buffer_allocation, NULL);

    if (res != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "VMA: Failed to create storage buffer: %s", vk_result_string(res));
    }

    vk.storage.buffer_ptr = (void*)((VmaAllocationInfo*)vk_storage_buffer_allocation)->pMappedData;
    vk.storage.memory = VK_NULL_HANDLE; // Not used with VMA

    Com_Memset(vk.storage.buffer_ptr, 0, size);

    SET_OBJECT_NAME(vk.storage.buffer, "storage buffer (VMA)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
    SET_OBJECT_NAME(vk.storage.descriptor, "storage buffer descriptor", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);

#else
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo alloc_info;
    VkBufferCreateInfo desc;
    uint32_t memory_type_bits;
    uint32_t memory_type;

    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;

    Com_Memset(&memory_requirements, 0, sizeof(memory_requirements));

    desc.size = size;
    desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &vk.storage.buffer));

    qvkGetBufferMemoryRequirements(vk.device, vk.storage.buffer, &memory_requirements);

    memory_type_bits = memory_requirements.memoryTypeBits;
    memory_type = find_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type;

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.storage.memory));
    VK_CHECK(qvkMapMemory(vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr));

    Com_Memset(vk.storage.buffer_ptr, 0, memory_requirements.size);

    qvkBindBufferMemory(vk.device, vk.storage.buffer, vk.storage.memory, 0);

    SET_OBJECT_NAME(vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
    SET_OBJECT_NAME(vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
    SET_OBJECT_NAME(vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif
}

void vk_release_vbo(void) {
#ifdef USE_VMA
    if (vk_vbo_allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vk.allocator, vk.vbo.vertex_buffer, vk_vbo_allocation);
        vk_vbo_allocation = VK_NULL_HANDLE;
    }
    vk.vbo.vertex_buffer = VK_NULL_HANDLE;
    vk.vbo.buffer_memory = VK_NULL_HANDLE; // Not used with VMA
#else
    if (vk.vbo.vertex_buffer)
        qvkDestroyBuffer(vk.device, vk.vbo.vertex_buffer, NULL);
    vk.vbo.vertex_buffer = VK_NULL_HANDLE;

    if (vk.vbo.buffer_memory)
        qvkFreeMemory(vk.device, vk.vbo.buffer_memory, NULL);
    vk.vbo.buffer_memory = VK_NULL_HANDLE;
#endif
}

qboolean vk_alloc_vbo(const byte *vbo_data, int vbo_size) {
    VkCommandBuffer command_buffer;
    VkBufferCopy copyRegion[1];

    vk_release_vbo();

#ifdef USE_VMA
    // Use VMA for better memory management
    VkBufferCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = vbo_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY  // Device-local for VBO
    };

    VkResult res = vmaCreateBuffer(vk.allocator, &desc, &allocCreateInfo,
        &vk.vbo.vertex_buffer, &vk_vbo_allocation, NULL);

    if (res != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "VMA: Failed to create VBO buffer: %s", vk_result_string(res));
    }

    vk.vbo.buffer_memory = VK_NULL_HANDLE; // Not used with VMA

#else
    VkMemoryRequirements vb_mem_reqs;
    VkMemoryAllocateInfo alloc_info;
    VkDeviceSize vertex_buffer_offset;
    VkDeviceSize allocationSize;
    uint32_t memory_type_bits;

    VkBufferCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = vbo_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };
    VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &vk.vbo.vertex_buffer));

    // memory requirements
    qvkGetBufferMemoryRequirements(vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs);
    vertex_buffer_offset = 0;
    allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
    memory_type_bits = vb_mem_reqs.memoryTypeBits;

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.allocationSize = allocationSize;
    alloc_info.memoryTypeIndex = find_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory));
    qvkBindBufferMemory(vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset);
#endif

    // copy data directly to staging buffer
    if ((VkDeviceSize)vbo_size > vk.staging_buffer.size) {
        ri.Error(ERR_FATAL, "VBO data size %d exceeds staging buffer size %lu", vbo_size, (unsigned long)vk.staging_buffer.size);
    }

    // copy data to staging buffer
    Com_Memcpy(vk.staging_buffer.ptr, vbo_data, vbo_size);

    // record copy command
    command_buffer = begin_command_buffer();

    copyRegion[0].srcOffset = 0;
    copyRegion[0].dstOffset = 0;
    copyRegion[0].size = vbo_size;
    qvkCmdCopyBuffer(command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, copyRegion);

    end_command_buffer(command_buffer, __func__);

    SET_OBJECT_NAME(vk.vbo.vertex_buffer, "VBO vertex buffer (VMA)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
#ifndef USE_VMA
    SET_OBJECT_NAME(vk.vbo.buffer_memory, "VBO buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif

    return qtrue;
}
