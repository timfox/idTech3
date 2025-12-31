# VMA (Vulkan Memory Allocator) Integration Guide

## Overview

VMA has been integrated into the idtech3 Vulkan renderer to provide better memory management, reduce fragmentation, and simplify allocation code.

## Installation

1. Download the VMA header file:
```bash
mkdir -p libs/vma
curl -L https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h -o libs/vma/vk_mem_alloc.h
```

Or use git submodule:
```bash
git submodule add https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git libs/vma
```

2. Build with VMA enabled (default):
```bash
cmake -DUSE_VMA=ON ..
make
```

## Features

- **Automatic memory management**: VMA handles memory type selection, alignment, and binding
- **Reduced fragmentation**: Better memory pooling and allocation strategies
- **Simplified code**: No need for manual memory requirements queries
- **Performance**: Optimized allocation paths and better cache locality
- **Debugging**: Built-in memory leak detection and statistics

## Usage

### Buffer Allocation Example

**Before (Manual):**
```c
VkBufferCreateInfo bufferInfo = {...};
VkMemoryRequirements memReqs;
qvkGetBufferMemoryRequirements(device, buffer, &memReqs);
VkMemoryAllocateInfo allocInfo = {...};
qvkAllocateMemory(device, &allocInfo, NULL, &memory);
qvkBindBufferMemory(device, buffer, memory, 0);
```

**After (VMA):**
```c
VkBufferCreateInfo bufferInfo = {...};
VmaAllocationCreateInfo allocCreateInfo = {
    .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
};
VmaAllocation allocation;
vmaCreateBuffer(vk.allocator, &bufferInfo, &allocCreateInfo, 
    &buffer, &allocation, NULL);
```

### Image Allocation Example

**Before (Manual):**
```c
VkImageCreateInfo imageInfo = {...};
VkMemoryRequirements memReqs;
qvkGetImageMemoryRequirements(device, image, &memReqs);
VkMemoryAllocateInfo allocInfo = {...};
qvkAllocateMemory(device, &allocInfo, NULL, &memory);
qvkBindImageMemory(device, image, memory, 0);
```

**After (VMA):**
```c
VkImageCreateInfo imageInfo = {...};
VmaAllocationCreateInfo allocCreateInfo = {
    .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
};
VmaAllocation allocation;
vmaCreateImage(vk.allocator, &imageInfo, &allocCreateInfo,
    &image, &allocation, NULL);
```

## Migration Status

- ✅ VMA initialization and cleanup
- ✅ Geometry buffer allocation (example implementation)
- ⏳ Storage buffer allocation
- ⏳ Staging buffer allocation
- ⏳ Image allocations
- ⏳ VBO allocations

## Memory Usage Hints

- `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`: For host-visible buffers (uniforms, staging)
- `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`: For device-local buffers (vertex/index buffers)
- `VMA_MEMORY_USAGE_AUTO`: Let VMA decide based on usage flags
- `VMA_ALLOCATION_CREATE_MAPPED_BIT`: For buffers that need persistent mapping

## Debugging

Enable VMA statistics:
```c
VmaStats stats;
vmaCalculateStats(vk.allocator, &stats);
// Print stats.total.usedBytes, stats.total.allocationCount, etc.
```

## Benefits

1. **Simplified code**: ~50% less code for allocations
2. **Better performance**: Reduced CPU overhead from fewer API calls
3. **Memory efficiency**: Better allocation strategies reduce fragmentation
4. **Easier debugging**: Built-in leak detection and statistics
5. **Future-proof**: Easy to add features like defragmentation

## Notes

- VMA is optional - the code falls back to manual allocation if `USE_VMA` is disabled
- All existing functionality is preserved
- No breaking changes to the API

