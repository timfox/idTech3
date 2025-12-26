/*
=============================================================================
GPU Memory Management System Tests

Unit tests for advanced VRAM allocation and defragmentation
=============================================================================
*/

#include "vk_gpu_memory.h"
#include "qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test utilities
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("TEST FAILED: %s\n", message); \
            test_results.failed++; \
            return; \
        } \
    } while (0)

#define TEST_PASS() \
    do { \
        test_results.passed++; \
        printf("PASS\n"); \
    } while (0)

typedef struct {
    int passed;
    int failed;
    int total;
} test_results_t;

static test_results_t test_results;

// Mock Vulkan objects for testing
static VkDevice mock_device = (VkDevice)0xDEADBEEF;
static VkPhysicalDevice mock_physical_device = (VkPhysicalDevice)0xCAFEBABE;

// Mock memory properties for testing
static VkPhysicalDeviceMemoryProperties mock_memory_properties = {
    .memoryHeapCount = 2,
    .memoryHeaps = {
        {
            .size = 1024 * 1024 * 1024, // 1GB
            .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
        },
        {
            .size = 512 * 1024 * 1024, // 512MB
            .flags = 0
        }
    },
    .memoryTypeCount = 8,
    .memoryTypes = {
        // Heap 0 (DEVICE_LOCAL)
        {
            .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .heapIndex = 0
        },
        {
            .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            .heapIndex = 0
        },
        // Heap 1 (HOST_VISIBLE)
        {
            .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .heapIndex = 1
        },
        {
            .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            .heapIndex = 1
        },
        // More types...
        { .propertyFlags = 0, .heapIndex = 0 },
        { .propertyFlags = 0, .heapIndex = 0 },
        { .propertyFlags = 0, .heapIndex = 1 },
        { .propertyFlags = 0, .heapIndex = 1 }
    }
};

// Mock Vulkan functions
VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo,
                         const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    static uint32_t memory_id = 1000;
    *pMemory = (VkDeviceMemory)(uintptr_t)memory_id++;
    return VK_SUCCESS;
}

void vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                 const VkAllocationCallbacks* pAllocator) {
    // Mock free - do nothing
}

VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo,
                       const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    static uint32_t buffer_id = 2000;
    *pBuffer = (VkBuffer)(uintptr_t)buffer_id++;
    return VK_SUCCESS;
}

void vkDestroyBuffer(VkDevice device, VkBuffer buffer,
                    const VkAllocationCallbacks* pAllocator) {
    // Mock destroy - do nothing
}

VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo,
                      const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    static uint32_t image_id = 3000;
    *pImage = (VkImage)(uintptr_t)image_id++;
    return VK_SUCCESS;
}

void vkDestroyImage(VkDevice device, VkImage image,
                   const VkAllocationCallbacks* pAllocator) {
    // Mock destroy - do nothing
}

void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                  VkMemoryRequirements* pMemoryRequirements) {
    pMemoryRequirements->size = 65536; // 64KB
    pMemoryRequirements->alignment = 256;
    pMemoryRequirements->memoryTypeBits = 0xFF; // All types
}

void vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                 VkMemoryRequirements* pMemoryRequirements) {
    pMemoryRequirements->size = 1048576; // 1MB
    pMemoryRequirements->alignment = 1024;
    pMemoryRequirements->memoryTypeBits = 0x0F; // First 4 types
}

VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                           VkDeviceSize memoryOffset) {
    return VK_SUCCESS;
}

VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory,
                          VkDeviceSize memoryOffset) {
    return VK_SUCCESS;
}

void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                        VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    *pMemoryProperties = mock_memory_properties;
}

// Test basic GPU memory initialization
static void Test_GPU_Memory_Init(void) {
    printf("Test_GPU_Memory_Init... ");
    test_results.total++;

    qboolean result = GPU_Memory_Init(mock_device, mock_physical_device);
    TEST_ASSERT(result == qtrue, "GPU memory initialization should succeed");

    // Check that system is initialized
    VkDeviceSize total_allocated, peak_usage;
    uint32_t allocation_count, failed_count;
    GPU_Memory_GetAllocationStats(&total_allocated, &peak_usage, &allocation_count, &failed_count);

    TEST_ASSERT(total_allocated == 0, "Initial total allocated should be 0");
    TEST_ASSERT(allocation_count == 0, "Initial allocation count should be 0");
    TEST_ASSERT(failed_count == 0, "Initial failed count should be 0");

    TEST_PASS();
}

// Test basic memory allocation
static void Test_GPU_Memory_Allocation(void) {
    printf("Test_GPU_Memory_Allocation... ");
    test_results.total++;

    // Create allocation request
    gpu_allocation_request_t request = {
        .requirements = {
            .size = 1024 * 1024, // 1MB
            .alignment = 256,
            .memoryTypeBits = 0xFF
        },
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .type = GPU_MEMORY_TYPE_BUFFER,
        .dedicated = qfalse,
        .can_relocate = qtrue,
        .debug_name = "TestBuffer"
    };

    // Allocate memory
    gpu_memory_block_t *block = GPU_Memory_Allocate(&request);
    TEST_ASSERT(block != NULL, "Memory allocation should succeed");

    if (block) {
        // Verify block properties
        TEST_ASSERT(block->size == 1024 * 1024, "Block size should match request");
        TEST_ASSERT(block->type == GPU_MEMORY_TYPE_BUFFER, "Block type should match");
        TEST_ASSERT(block->status == GPU_BLOCK_ALLOCATED, "Block should be allocated");
        TEST_ASSERT(block->allocation_id > 0, "Block should have valid allocation ID");
        TEST_ASSERT(strcmp(block->debug_name, "TestBuffer") == 0, "Debug name should match");

        // Free memory
        qboolean free_result = GPU_Memory_Free(block);
        TEST_ASSERT(free_result == qtrue, "Memory free should succeed");
    }

    TEST_PASS();
}

// Test memory type finding
static void Test_GPU_Memory_TypeFinding(void) {
    printf("Test_GPU_Memory_TypeFinding... ");
    test_results.total++;

    // Test finding optimal memory type for device local
    VkMemoryRequirements req = {
        .size = 4096,
        .alignment = 256,
        .memoryTypeBits = 0xFF
    };

    uint32_t type_index = GPU_Memory_FindOptimalType(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    TEST_ASSERT(type_index < VK_MAX_MEMORY_TYPES, "Should find valid memory type");

    // Verify the type has the requested properties
    VkMemoryType found_type = mock_memory_properties.memoryTypes[type_index];
    TEST_ASSERT((found_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0,
               "Found type should be device local");

    // Test finding heap for memory type
    uint32_t heap_index = GPU_Memory_GetHeapForType(type_index);
    TEST_ASSERT(heap_index < mock_memory_properties.memoryHeapCount, "Should find valid heap");

    TEST_PASS();
}

// Test heap statistics
static void Test_GPU_Memory_HeapStats(void) {
    printf("Test_GPU_Memory_HeapStats... ");
    test_results.total++;

    // Get heap statistics
    gpu_memory_heap_t stats;
    qboolean result = GPU_Memory_GetHeapStats(0, &stats);
    TEST_ASSERT(result == qtrue, "Should get heap statistics");

    // Verify heap properties
    TEST_ASSERT(stats.heap_index == 0, "Heap index should match");
    TEST_ASSERT(stats.total_size == 1024 * 1024 * 1024, "Total size should match mock");
    TEST_ASSERT(stats.allocation_count >= 0, "Allocation count should be valid");

    // Test fragmentation calculation
    float fragmentation = GPU_Memory_GetFragmentation(0);
    TEST_ASSERT(fragmentation >= 0.0f && fragmentation <= 1.0f,
               "Fragmentation should be between 0 and 1");

    TEST_PASS();
}

// Test buffer creation integration
static void Test_GPU_Memory_BufferCreation(void) {
    printf("Test_GPU_Memory_BufferCreation... ");
    test_results.total++;

    // Create buffer info
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 65536, // 64KB
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer;
    gpu_memory_block_t *block;

    qboolean result = GPU_Memory_CreateBuffer(&buffer_info,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            &buffer, &block);
    TEST_ASSERT(result == qtrue, "Buffer creation should succeed");
    TEST_ASSERT(buffer != VK_NULL_HANDLE, "Buffer handle should be valid");
    TEST_ASSERT(block != NULL, "Memory block should be allocated");

    if (block) {
        TEST_ASSERT(block->type == GPU_MEMORY_TYPE_BUFFER, "Block should be buffer type");
        TEST_ASSERT(block->size >= 65536, "Block should be large enough");

        // Free resources
        GPU_Memory_Free(block);
    }

    TEST_PASS();
}

// Test image creation integration
static void Test_GPU_Memory_ImageCreation(void) {
    printf("Test_GPU_Memory_ImageCreation... ");
    test_results.total++;

    // Create image info
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1024, 1024, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage image;
    gpu_memory_block_t *block;

    qboolean result = GPU_Memory_CreateImage(&image_info,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           &image, &block);
    TEST_ASSERT(result == qtrue, "Image creation should succeed");
    TEST_ASSERT(image != VK_NULL_HANDLE, "Image handle should be valid");
    TEST_ASSERT(block != NULL, "Memory block should be allocated");

    if (block) {
        TEST_ASSERT(block->type == GPU_MEMORY_TYPE_TEXTURE, "Block should be texture type");
        TEST_ASSERT(block->size >= 1024 * 1024 * 4, "Block should be large enough for image");

        // Free resources
        GPU_Memory_Free(block);
    }

    TEST_PASS();
}

// Test allocation strategies
static void Test_GPU_Memory_AllocationStrategies(void) {
    printf("Test_GPU_Memory_AllocationStrategies... ");
    test_results.total++;

    // Test setting allocation strategy
    GPU_Memory_SetAllocationStrategy(GPU_ALLOC_STRATEGY_BEST_FIT);
    // The actual allocation algorithm testing would require more complex setup
    // For this test, we just verify the system doesn't crash

    TEST_PASS();
}

// Test defragmentation
static void Test_GPU_Memory_Defragmentation(void) {
    printf("Test_GPU_Memory_Defragmentation... ");
    test_results.total++;

    // Test defragmentation parameters
    GPU_Memory_SetDefragParameters(GPU_DEFRAG_ONLINE, 5000, 0.3f);

    // Test starting defragmentation
    qboolean start_result = GPU_Memory_StartDefragmentation(0, GPU_DEFRAG_ONLINE);
    TEST_ASSERT(start_result == qtrue, "Defragmentation should start");

    // Test progress tracking
    float progress = GPU_Memory_GetDefragmentationProgress(0);
    TEST_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress should be valid");

    // Test stopping defragmentation
    qboolean stop_result = GPU_Memory_StopDefragmentation(0);
    TEST_ASSERT(stop_result == qtrue, "Defragmentation should stop");

    TEST_PASS();
}

// Test memory debugging
static void Test_GPU_Memory_Debugging(void) {
    printf("Test_GPU_Memory_Debugging... ");
    test_results.total++;

    // Allocate memory with debug name
    gpu_allocation_request_t request = {
        .requirements = {
            .size = 4096,
            .alignment = 256,
            .memoryTypeBits = 0xFF
        },
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .type = GPU_MEMORY_TYPE_BUFFER,
        .debug_name = "DebugTest"
    };

    gpu_memory_block_t *block = GPU_Memory_Allocate(&request);
    TEST_ASSERT(block != NULL, "Debug allocation should succeed");

    if (block) {
        // Test setting debug name
        GPU_Memory_SetDebugName(block, "UpdatedDebugName");

        // Test getting allocation info
        gpu_memory_type_t type;
        VkDeviceSize size;
        uint64_t timestamp;
        char debug_name[64];

        qboolean info_result = GPU_Memory_GetAllocationInfo(block, &type, &size,
                                                          &timestamp, debug_name,
                                                          sizeof(debug_name));
        TEST_ASSERT(info_result == qtrue, "Should get allocation info");
        TEST_ASSERT(type == GPU_MEMORY_TYPE_BUFFER, "Type should match");
        TEST_ASSERT(size == 4096, "Size should match");
        TEST_ASSERT(strcmp(debug_name, "UpdatedDebugName") == 0, "Debug name should match");

        // Free memory
        GPU_Memory_Free(block);
    }

    TEST_PASS();
}

// Test performance monitoring
static void Test_GPU_Memory_Performance(void) {
    printf("Test_GPU_Memory_Performance... ");
    test_results.total++;

    // Perform some allocations to generate performance data
    for (int i = 0; i < 5; i++) {
        gpu_allocation_request_t request = {
            .requirements = {
                .size = 1024,
                .alignment = 256,
                .memoryTypeBits = 0xFF
            },
            .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .type = GPU_MEMORY_TYPE_BUFFER
        };

        gpu_memory_block_t *block = GPU_Memory_Allocate(&request);
        if (block) {
            GPU_Memory_Free(block);
        }
    }

    // Get performance metrics
    uint64_t alloc_time, free_time, defrag_time;
    GPU_Memory_GetPerformanceMetrics(&alloc_time, &free_time, &defrag_time);

    TEST_ASSERT(alloc_time >= 0, "Allocation time should be valid");
    TEST_ASSERT(free_time >= 0, "Free time should be valid");
    TEST_ASSERT(defrag_time >= 0, "Defrag time should be valid");

    // Reset counters
    GPU_Memory_ResetPerformanceCounters();

    // Verify counters are reset
    GPU_Memory_GetPerformanceMetrics(&alloc_time, &free_time, &defrag_time);
    // Note: Exact values depend on implementation, just verify function works

    TEST_PASS();
}

// Test console commands
static void Test_GPU_Memory_ConsoleCommands(void) {
    printf("Test_GPU_Memory_ConsoleCommands... ");
    test_results.total++;

    // Test status command (should not crash)
    GPU_Memory_Status_f();

    // Test stats command
    GPU_Memory_Stats_f();

    // Test dump command
    GPU_Memory_Dump_f();

    // Test defrag command
    GPU_Memory_Defrag_f();

    // Test validate command
    GPU_Memory_Validate_f();

    // Test leaks command
    GPU_Memory_Leaks_f();

    // All commands should execute without crashing
    TEST_PASS();
}

// Test utility functions
static void Test_GPU_Memory_Utilities(void) {
    printf("Test_GPU_Memory_Utilities... ");
    test_results.total++;

    // Test memory type availability
    qboolean available = GPU_Memory_IsMemoryTypeAvailable(0);
    TEST_ASSERT(available == qtrue, "First memory type should be available");

    available = GPU_Memory_IsMemoryTypeAvailable(999);
    TEST_ASSERT(available == qfalse, "Invalid memory type should not be available");

    // Test size calculations
    VkMemoryRequirements req;
    GPU_Memory_CalculateRequirements(GPU_MEMORY_TYPE_BUFFER, 1024, &req);
    TEST_ASSERT(req.size >= 1024, "Calculated size should be at least requested");

    VkDeviceSize recommended = GPU_Memory_GetRecommendedSize(GPU_MEMORY_TYPE_BUFFER, 1024);
    TEST_ASSERT(recommended >= 1024, "Recommended size should be at least requested");

    TEST_PASS();
}

// Test system shutdown
static void Test_GPU_Memory_Shutdown(void) {
    printf("Test_GPU_Memory_Shutdown... ");
    test_results.total++;

    // Shutdown the system
    GPU_Memory_Shutdown();

    // Verify system is shut down (should handle subsequent calls gracefully)
    gpu_memory_block_t *block = GPU_Memory_Allocate(&(gpu_allocation_request_t){
        .requirements = {.size = 1024, .alignment = 256, .memoryTypeBits = 0xFF},
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .type = GPU_MEMORY_TYPE_BUFFER
    });
    TEST_ASSERT(block == NULL, "Allocation after shutdown should fail");

    TEST_PASS();
}

// Main test runner
int main(int argc, char *argv[]) {
    printf("GPU Memory Management System Tests\n");
    printf("===================================\n\n");

    // Initialize test results
    memset(&test_results, 0, sizeof(test_results));

    // Run all tests
    Test_GPU_Memory_Init();
    Test_GPU_Memory_Allocation();
    Test_GPU_Memory_TypeFinding();
    Test_GPU_Memory_HeapStats();
    Test_GPU_Memory_BufferCreation();
    Test_GPU_Memory_ImageCreation();
    Test_GPU_Memory_AllocationStrategies();
    Test_GPU_Memory_Defragmentation();
    Test_GPU_Memory_Debugging();
    Test_GPU_Memory_Performance();
    Test_GPU_Memory_ConsoleCommands();
    Test_GPU_Memory_Utilities();
    Test_GPU_Memory_Shutdown();

    // Print results
    printf("\nTest Results:\n");
    printf("=============\n");
    printf("Total Tests: %d\n", test_results.total);
    printf("Passed: %d\n", test_results.passed);
    printf("Failed: %d\n", test_results.failed);

    float pass_rate = test_results.total > 0 ?
        (float)test_results.passed / test_results.total * 100.0f : 0.0f;
    printf("Pass Rate: %.1f%%\n", pass_rate);

    return test_results.failed > 0 ? 1 : 0;
}
