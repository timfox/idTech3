# Swapchain Recreation and Shutdown Cleanup Review

## Overview
This document summarizes the review and improvements made to swapchain recreation logic and Vulkan resource cleanup during shutdown.

## Issues Identified and Fixed

### 1. Swapchain Recreation Robustness

#### Issue: Partial Resource Creation on Failure
**Problem**: If swapchain recreation failed partway through (e.g., during image view or semaphore creation), some resources would be created but not all, leaving the system in an inconsistent state.

**Fix**: Added proper cleanup in `vk_create_swapchain_safe()`:
- If image view creation fails, all previously created image views and semaphores are destroyed before returning
- If semaphore creation fails, all previously created resources (image views and semaphores) are cleaned up
- Ensures no resource leaks on partial failure

**Location**: `src/renderers/vulkan/vk.c` lines 1404-1427

#### Issue: Framebuffer Creation Failure Handling
**Problem**: If framebuffer creation failed after swapchain was successfully created, the swapchain would exist but framebuffers wouldn't, potentially causing rendering issues.

**Fix**: Added validation and logging after framebuffer creation:
- Verifies that main framebuffers were created successfully
- Logs warnings if any framebuffers failed to create
- System can continue with valid swapchain even if some framebuffers are missing (rendering will be limited)

**Location**: `src/renderers/vulkan/vk.c` lines 10690-10708

#### Issue: Swapchain Recreation Error Handling
**Problem**: In `vk_swapchain.cpp`, swapchain recreation during `vk_acquire_next_image()` and `vk_present_image()` didn't properly handle failures or device lost conditions.

**Fix**: 
- Added error checking after swapchain recreation attempts
- Added device lost detection and memory tracking reset
- Improved error messages and logging
- Return appropriate error codes instead of silently failing

**Location**: `src/renderers/vulkan/vk_swapchain.cpp` lines 330-338, 362-372

### 2. Shutdown Cleanup

#### Issue: Missing Command Pool Cleanup
**Problem**: The main command pool (`vk.command_pool`) was not explicitly destroyed during shutdown, relying on device destruction to clean it up.

**Fix**: Added explicit command pool destruction:
- Calls `vk_destroy_command_pool()` before device destruction
- Ensures proper cleanup order (command buffers freed before pool destruction)
- Better resource tracking and validation

**Location**: `src/renderers/vulkan/vk.c` lines 9870-9875

#### Issue: Missing Descriptor Pool Cleanup
**Problem**: The descriptor pool (`vk.descriptor_pool`) was not explicitly destroyed during shutdown.

**Fix**: Added explicit descriptor pool destruction:
- Destroys descriptor pool before device destruction
- Proper cleanup order maintained
- Added logging for verification

**Location**: `src/renderers/vulkan/vk.c` lines 9877-9882

#### Issue: Missing Shader Module Cleanup
**Problem**: Shader modules were not explicitly destroyed during shutdown, relying on device destruction.

**Fix**: Added explicit cleanup for common shader modules:
- Destroys common shader modules (dot, fog, color) explicitly
- Better resource tracking and validation
- Note: Other shader modules are destroyed when pipelines are destroyed or with device destruction

**Location**: `src/renderers/vulkan/vk.c` lines 9884-9908

#### Issue: Resource Cleanup Order
**Problem**: Framebuffers and swapchain were not destroyed in the correct order during shutdown.

**Fix**: Ensured proper cleanup order:
- Framebuffers destroyed before swapchain (required by Vulkan spec)
- Command pool destroyed before device destruction
- Descriptor pool destroyed before device destruction
- Added comments documenting cleanup order requirements

**Location**: `src/renderers/vulkan/vk.c` lines 9868-9908

## Improvements Summary

### Swapchain Recreation
1. ✅ Proper cleanup on partial failure during swapchain creation
2. ✅ Validation and error handling after framebuffer creation
3. ✅ Improved error handling in acquire/present paths
4. ✅ Device lost detection and recovery
5. ✅ Better logging and error messages

### Shutdown Cleanup
1. ✅ Explicit command pool destruction
2. ✅ Explicit descriptor pool destruction
3. ✅ Explicit shader module cleanup (common modules)
4. ✅ Proper resource cleanup order
5. ✅ Device lost handling during shutdown

## Testing Recommendations

1. **Swapchain Recreation**:
   - Test window resize (triggers swapchain recreation)
   - Test device lost recovery
   - Test out-of-memory conditions during recreation
   - Verify no resource leaks on partial failures

2. **Shutdown Cleanup**:
   - Verify all resources are properly destroyed
   - Test shutdown during device lost state
   - Verify no validation layer errors on shutdown
   - Check for memory leaks using validation layers

## Notes

- Shader modules are automatically destroyed when the device is destroyed, but explicit cleanup helps with resource tracking and validation
- Command pools must be destroyed before device destruction
- Framebuffers must be destroyed before swapchain destruction (Vulkan spec requirement)
- All cleanup functions handle device lost state gracefully

## Related Files

- `src/renderers/vulkan/vk.c` - Main Vulkan initialization and shutdown
- `src/renderers/vulkan/vk_swapchain.cpp` - Swapchain management
- `src/renderers/vulkan/vk_framebuffer.c` - Framebuffer management
- `src/renderers/vulkan/vk_command_buffers.cpp` - Command pool management
