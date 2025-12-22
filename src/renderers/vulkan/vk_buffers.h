#ifndef __VK_BUFFERS_H__
#define __VK_BUFFERS_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "q_shared.h"
#include "vk.h"

#ifdef USE_VMA
#include "vk_mem_alloc.h"

// VMA buffer allocation structures
typedef struct {
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
} VmaBufferAllocation;

extern VmaBufferAllocation vk_geometry_buffer_vma[NUM_COMMAND_BUFFERS];
extern VmaAllocation vk_geometry_buffer_memory_vma;
#endif

// Buffer management function declarations
void vk_release_geometry_buffers(void);
void vk_create_geometry_buffers(VkDeviceSize size);
void vk_create_storage_buffer(uint32_t size);
void vk_release_vbo(void);
qboolean vk_alloc_vbo(const byte *vbo_data, int vbo_size);

#endif // __VK_BUFFERS_H__
