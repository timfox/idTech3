# Vulkan Memory Allocator (VMA)

This directory contains the Vulkan Memory Allocator library.

## Installation

VMA is a single-header library. To install it:

```bash
# Download the latest VMA header
curl -L https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h -o vk_mem_alloc.h
```

Or use git submodule:
```bash
git submodule add https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git vma
```

## Usage

VMA is included via `#define VMA_IMPLEMENTATION` in one source file (vk.c).

