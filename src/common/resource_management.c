/*
=============================================================================
Resource Management Framework Implementation

RAII patterns and automatic resource cleanup for C.
=============================================================================
*/

#include "resource_management.h"
#include "error_handling.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// Global resource manager instance
resource_manager_t resource_manager = {0};

// Resource scope pool for efficient allocation
#define MAX_RESOURCE_SCOPES 64
static resource_scope_t resource_scope_pool[MAX_RESOURCE_SCOPES];
static qboolean scope_used[MAX_RESOURCE_SCOPES] = {qfalse};

// Resource handle pool
#define MAX_RESOURCE_HANDLES 1024
static resource_handle_t resource_handle_pool[MAX_RESOURCE_HANDLES];
static qboolean handle_used[MAX_RESOURCE_HANDLES] = {qfalse};

// Thread-local current scope (for thread safety)
__thread resource_scope_t *thread_current_scope = NULL;

// Resource type strings
static const char *resource_type_strings[RESOURCE_TYPE_COUNT] = {
    "Memory",
    "File",
    "Socket",
    "Mutex",
    "Thread",
    "VulkanBuffer",
    "VulkanImage",
    "VulkanShader",
    "OpenALSource",
    "OpenALBuffer",
    "Custom"
};

/*
=============================================================================
Resource Management API Implementation
=============================================================================
*/

qboolean Resource_Init(void) {
    if (resource_manager.current_scope) {
        return qtrue; // Already initialized
    }

    memset(&resource_manager, 0, sizeof(resource_manager_t));
    memset(resource_scope_pool, 0, sizeof(resource_scope_pool));
    memset(scope_used, 0, sizeof(scope_used));
    memset(resource_handle_pool, 0, sizeof(resource_handle_pool));
    memset(handle_used, 0, sizeof(handle_used));

    // Create global scope
    resource_manager.global_scope = Resource_AllocateScope("global");
    if (!resource_manager.global_scope) {
        Com_Printf("Failed to create global resource scope\n");
        return qfalse;
    }

    resource_manager.current_scope = resource_manager.global_scope;
    thread_current_scope = resource_manager.current_scope;
    resource_manager.auto_cleanup_enabled = qtrue;
    resource_manager.leak_detection_enabled = qtrue;

    Com_Printf("Resource management framework initialized with RAII patterns\n");

    return qtrue;
}

void Resource_Shutdown(void) {
    if (!resource_manager.current_scope) {
        return; // Not initialized
    }

    // Clean up all scopes starting from current scope
    while (resource_manager.current_scope && resource_manager.current_scope != resource_manager.global_scope) {
        Resource_ExitScope();
    }

    // Clean up global scope
    if (resource_manager.global_scope) {
        Resource_CleanupAllInScope(resource_manager.global_scope);
        Resource_FreeScope(resource_manager.global_scope);
        resource_manager.global_scope = NULL;
    }

    resource_manager.current_scope = NULL;
    thread_current_scope = NULL;

    // Report any leaks
    resource_leak_info_t *leaks = NULL;
    uint32_t leak_count = Resource_DetectLeaks(&leaks);
    if (leak_count > 0) {
        Com_Printf("WARNING: %u resource leaks detected during shutdown\n", leak_count);
        Resource_ReportLeaks();
    }

    Com_Printf("Resource management framework shutdown\n");
}

/*
=============================================================================
Scope Management
=============================================================================
*/

resource_scope_t* Resource_AllocateScope(const char *scope_name) {
    for (int i = 0; i < MAX_RESOURCE_SCOPES; i++) {
        if (!scope_used[i]) {
            scope_used[i] = qtrue;
            memset(&resource_scope_pool[i], 0, sizeof(resource_scope_t));

            resource_scope_t *scope = &resource_scope_pool[i];
            Q_strncpyz((char*)scope->scope_name, scope_name, sizeof(scope->scope_name));
            scope->max_resources = 64;
            scope->resources = (resource_handle_t**)malloc(sizeof(resource_handle_t*) * scope->max_resources);

            if (!scope->resources) {
                scope_used[i] = qfalse;
                return NULL;
            }

            memset(scope->resources, 0, sizeof(resource_handle_t*) * scope->max_resources);
            scope->auto_cleanup_on_exit = qtrue;

            return scope;
        }
    }

    // Fallback: allocate from heap
    Com_Printf("Warning: Resource scope pool exhausted, allocating from heap\n");
    resource_scope_t *scope = (resource_scope_t*)malloc(sizeof(resource_scope_t));
    if (scope) {
        memset(scope, 0, sizeof(resource_scope_t));
        Q_strncpyz((char*)scope->scope_name, scope_name, sizeof(scope->scope_name));
        scope->max_resources = 64;
        scope->resources = (resource_handle_t**)malloc(sizeof(resource_handle_t*) * scope->max_resources);
        if (!scope->resources) {
            free(scope);
            return NULL;
        }
        memset(scope->resources, 0, sizeof(resource_handle_t*) * scope->max_resources);
        scope->auto_cleanup_on_exit = qtrue;
    }

    return scope;
}

void Resource_FreeScope(resource_scope_t *scope) {
    if (!scope) return;

    // Check if it's from our pool
    ptrdiff_t offset = (char*)scope - (char*)resource_scope_pool;
    if (offset >= 0 && offset < sizeof(resource_scope_pool)) {
        int index = offset / sizeof(resource_scope_t);
        if (index >= 0 && index < MAX_RESOURCE_SCOPES) {
            if (scope->resources) {
                free(scope->resources);
                scope->resources = NULL;
            }
            scope_used[index] = qfalse;
            return;
        }
    }

    // It was allocated from heap
    if (scope->resources) {
        free(scope->resources);
    }
    free(scope);
}

resource_scope_t* Resource_CreateScope(const char *scope_name) {
    resource_scope_t *scope = Resource_AllocateScope(scope_name);
    if (scope) {
        scope->parent_scope = resource_manager.current_scope;
    }
    return scope;
}

void Resource_EnterScope(resource_scope_t *scope) {
    if (!scope) return;

    scope->parent_scope = resource_manager.current_scope;
    resource_manager.current_scope = scope;
    thread_current_scope = scope;
    resource_manager.active_scopes++;
}

void Resource_ExitScope(void) {
    if (!resource_manager.current_scope || resource_manager.current_scope == resource_manager.global_scope) {
        return; // Cannot exit global scope
    }

    resource_scope_t *current = resource_manager.current_scope;

    // Auto cleanup resources if enabled
    if (current->auto_cleanup_on_exit && resource_manager.auto_cleanup_enabled) {
        Resource_CleanupAllInScope(current);
    }

    // Restore parent scope
    resource_manager.current_scope = current->parent_scope;
    thread_current_scope = resource_manager.current_scope;
    resource_manager.active_scopes--;

    // Free the scope
    Resource_FreeScope(current);
}

void Resource_ExitScopeTo(resource_scope_t *target_scope) {
    while (resource_manager.current_scope && resource_manager.current_scope != target_scope) {
        Resource_ExitScope();
    }
}

/*
=============================================================================
Resource Registration and Management
=============================================================================
*/

resource_handle_t* Resource_AllocateHandle(void) {
    for (int i = 0; i < MAX_RESOURCE_HANDLES; i++) {
        if (!handle_used[i]) {
            handle_used[i] = qtrue;
            memset(&resource_handle_pool[i], 0, sizeof(resource_handle_t));
            return &resource_handle_pool[i];
        }
    }

    // Fallback: allocate from heap
    Com_Printf("Warning: Resource handle pool exhausted, allocating from heap\n");
    resource_handle_t *handle = (resource_handle_t*)malloc(sizeof(resource_handle_t));
    if (handle) {
        memset(handle, 0, sizeof(resource_handle_t));
    }

    return handle;
}

void Resource_FreeHandle(resource_handle_t *handle) {
    if (!handle) return;

    // Check if it's from our pool
    ptrdiff_t offset = (char*)handle - (char*)resource_handle_pool;
    if (offset >= 0 && offset < sizeof(resource_handle_pool)) {
        int index = offset / sizeof(resource_handle_t);
        if (index >= 0 && index < MAX_RESOURCE_HANDLES) {
            handle_used[index] = qfalse;
            return;
        }
    }

    // It was allocated from heap
    free(handle);
}

resource_handle_t* Resource_Register(resource_type_t type, void *resource_data,
                                   resource_cleanup_fn cleanup_fn, const char *name) {
    if (!resource_manager.current_scope) {
        Com_Printf("Warning: No active resource scope, resource will not be managed\n");
        return NULL;
    }

    resource_handle_t *handle = Resource_AllocateHandle();
    if (!handle) {
        Com_Printf("Failed to allocate resource handle\n");
        return NULL;
    }

    handle->type = type;
    handle->resource_data = resource_data;
    handle->cleanup_fn = cleanup_fn;
    handle->auto_cleanup = qtrue;
    handle->allocation_time = Sys_Milliseconds();

    if (name) {
        handle->resource_name = name; // Assume string is persistent
    }

    // Add to current scope
    resource_scope_t *scope = resource_manager.current_scope;
    if (scope->resource_count < scope->max_resources) {
        scope->resources[scope->resource_count++] = handle;
    } else {
        Com_Printf("Warning: Scope resource limit reached, resource may leak\n");
        Resource_FreeHandle(handle);
        return NULL;
    }

    resource_manager.total_allocated_resources++;

    return handle;
}

void Resource_Unregister(resource_handle_t *handle) {
    if (!handle) return;

    // Remove from scope
    resource_scope_t *scope = resource_manager.current_scope;
    for (uint32_t i = 0; i < scope->resource_count; i++) {
        if (scope->resources[i] == handle) {
            // Shift remaining resources
            for (uint32_t j = i; j < scope->resource_count - 1; j++) {
                scope->resources[j] = scope->resources[j + 1];
            }
            scope->resources[--scope->resource_count] = NULL;
            break;
        }
    }

    Resource_FreeHandle(handle);
}

void Resource_Cleanup(resource_handle_t *handle) {
    if (!handle || !handle->cleanup_fn) return;

    // Call cleanup function
    handle->cleanup_fn(handle->resource_data);

    // Mark as cleaned up
    handle->auto_cleanup = qfalse;
    handle->resource_data = NULL;
}

void Resource_CleanupAllInScope(resource_scope_t *scope) {
    if (!scope) return;

    for (uint32_t i = 0; i < scope->resource_count; i++) {
        resource_handle_t *handle = scope->resources[i];
        if (handle && handle->auto_cleanup) {
            Resource_Cleanup(handle);
        }
    }

    // Clear the scope
    memset(scope->resources, 0, sizeof(resource_handle_t*) * scope->resource_count);
    scope->resource_count = 0;
}

/*
=============================================================================
Memory Resource Management
=============================================================================
*/

void Resource_CleanupMemory(void *data) {
    if (!data) return;

    memory_resource_t *mem_res = (memory_resource_t*)data;

    if (mem_res->zero_on_free && mem_res->ptr) {
        memset(mem_res->ptr, 0, mem_res->size);
    }

    if (mem_res->ptr) {
        free(mem_res->ptr);
        mem_res->ptr = NULL;
    }

    free(mem_res);
}

resource_handle_t* Resource_AllocateMemory(size_t size, const char *name) {
    memory_resource_t *mem_res = (memory_resource_t*)malloc(sizeof(memory_resource_t));
    if (!mem_res) return NULL;

    mem_res->ptr = malloc(size);
    if (!mem_res->ptr) {
        free(mem_res);
        return NULL;
    }

    mem_res->size = size;
    mem_res->zero_on_free = qfalse;

    return Resource_Register(RESOURCE_TYPE_MEMORY, mem_res, Resource_CleanupMemory, name);
}

resource_handle_t* Resource_AllocateMemoryZero(size_t size, const char *name) {
    resource_handle_t *handle = Resource_AllocateMemory(size, name);
    if (handle) {
        memory_resource_t *mem_res = (memory_resource_t*)handle->resource_data;
        memset(mem_res->ptr, 0, size);
        mem_res->zero_on_free = qtrue;
    }
    return handle;
}

void* Resource_GetMemoryPtr(resource_handle_t *handle) {
    if (!handle || handle->type != RESOURCE_TYPE_MEMORY) return NULL;
    memory_resource_t *mem_res = (memory_resource_t*)handle->resource_data;
    return mem_res->ptr;
}

/*
=============================================================================
File Resource Management
=============================================================================
*/

void Resource_CleanupFile(void *data) {
    if (!data) return;

    file_resource_t *file_res = (file_resource_t*)data;

    if (file_res->close_on_cleanup && file_res->file) {
        fclose(file_res->file);
        file_res->file = NULL;
    }

    free(file_res);
}

resource_handle_t* Resource_OpenFile(const char *filename, const char *mode, const char *resource_name) {
    file_resource_t *file_res = (file_resource_t*)malloc(sizeof(file_resource_t));
    if (!file_res) return NULL;

    file_res->file = fopen(filename, mode);
    if (!file_res->file) {
        free(file_res);
        return NULL;
    }

    Q_strncpyz(file_res->filename, filename, sizeof(file_res->filename));
    file_res->close_on_cleanup = qtrue;

    return Resource_Register(RESOURCE_TYPE_FILE, file_res, Resource_CleanupFile, resource_name);
}

FILE* Resource_GetFileHandle(resource_handle_t *handle) {
    if (!handle || handle->type != RESOURCE_TYPE_FILE) return NULL;
    file_resource_t *file_res = (file_resource_t*)handle->resource_data;
    return file_res->file;
}

/*
=============================================================================
Network Resource Management
=============================================================================
*/

void Resource_CleanupSocket(void *data) {
    if (!data) return;

    socket_resource_t *socket_res = (socket_resource_t*)data;

    if (socket_res->socket_fd >= 0) {
        close(socket_res->socket_fd);
        socket_res->socket_fd = -1;
    }

    free(socket_res);
}

resource_handle_t* Resource_CreateSocket(int socket_fd, const char *address, uint16_t port, const char *name) {
    socket_resource_t *socket_res = (socket_resource_t*)malloc(sizeof(socket_resource_t));
    if (!socket_res) return NULL;

    socket_res->socket_fd = socket_fd;

    if (address) {
        Q_strncpyz(socket_res->address, address, sizeof(socket_res->address));
    }

    socket_res->port = port;

    return Resource_Register(RESOURCE_TYPE_SOCKET, socket_res, Resource_CleanupSocket, name);
}

/*
=============================================================================
Vulkan Resource Management
=============================================================================
*/

void Resource_CleanupVulkanBuffer(void *data) {
    if (!data) return;

    vulkan_buffer_resource_t *buffer_res = (vulkan_buffer_resource_t*)data;

    // Note: In a real implementation, this would call Vulkan cleanup functions
    // For now, just mark as cleaned up
    memset(buffer_res, 0, sizeof(vulkan_buffer_resource_t));
    free(buffer_res);
}

resource_handle_t* Resource_CreateVulkanBuffer(VkBuffer buffer, VkDeviceMemory memory,
                                             VkDeviceSize size, VkBufferUsageFlags usage,
                                             const char *name) {
    vulkan_buffer_resource_t *buffer_res = (vulkan_buffer_resource_t*)malloc(sizeof(vulkan_buffer_resource_t));
    if (!buffer_res) return NULL;

    buffer_res->buffer = buffer;
    buffer_res->memory = memory;
    buffer_res->size = size;
    buffer_res->usage = usage;

    return Resource_Register(RESOURCE_TYPE_VULKAN_BUFFER, buffer_res, Resource_CleanupVulkanBuffer, name);
}

void Resource_CleanupVulkanImage(void *data) {
    if (!data) return;

    vulkan_image_resource_t *image_res = (vulkan_image_resource_t*)data;

    // Vulkan cleanup would go here
    memset(image_res, 0, sizeof(vulkan_image_resource_t));
    free(image_res);
}

resource_handle_t* Resource_CreateVulkanImage(VkImage image, VkImageView view,
                                            VkDeviceMemory memory, const char *name) {
    vulkan_image_resource_t *image_res = (vulkan_image_resource_t*)malloc(sizeof(vulkan_image_resource_t));
    if (!image_res) return NULL;

    image_res->image = image;
    image_res->view = view;
    image_res->memory = memory;
    image_res->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    return Resource_Register(RESOURCE_TYPE_VULKAN_IMAGE, image_res, Resource_CleanupVulkanImage, name);
}

void Resource_CleanupVulkanShader(void *data) {
    if (!data) return;

    vulkan_shader_resource_t *shader_res = (vulkan_shader_resource_t*)data;

    // Vulkan shader cleanup would go here
    memset(shader_res, 0, sizeof(vulkan_shader_resource_t));
    free(shader_res);
}

resource_handle_t* Resource_CreateVulkanShader(VkShaderModule module, const char *shader_name) {
    vulkan_shader_resource_t *shader_res = (vulkan_shader_resource_t*)malloc(sizeof(vulkan_shader_resource_t));
    if (!shader_res) return NULL;

    shader_res->module = module;
    Q_strncpyz(shader_res->shader_name, shader_name, sizeof(shader_res->shader_name));

    return Resource_Register(RESOURCE_TYPE_VULKAN_SHADER, shader_res, Resource_CleanupVulkanShader, shader_name);
}

/*
=============================================================================
OpenAL Resource Management
=============================================================================
*/

void Resource_CleanupOpenALSource(void *data) {
    if (!data) return;

    openal_source_resource_t *source_res = (openal_source_resource_t*)data;

    // OpenAL source cleanup would go here
    memset(source_res, 0, sizeof(openal_source_resource_t));
    free(source_res);
}

resource_handle_t* Resource_CreateOpenALSource(ALuint source, const char *name) {
    openal_source_resource_t *source_res = (openal_source_resource_t*)malloc(sizeof(openal_source_resource_t));
    if (!source_res) return NULL;

    source_res->source = source;
    Q_strncpyz(source_res->source_name, name, sizeof(source_res->source_name));

    return Resource_Register(RESOURCE_TYPE_OPENAL_SOURCE, source_res, Resource_CleanupOpenALSource, name);
}

void Resource_CleanupOpenALBuffer(void *data) {
    if (!data) return;

    openal_buffer_resource_t *buffer_res = (openal_buffer_resource_t*)data;

    // OpenAL buffer cleanup would go here
    memset(buffer_res, 0, sizeof(openal_buffer_resource_t));
    free(buffer_res);
}

resource_handle_t* Resource_CreateOpenALBuffer(ALuint buffer, const char *name) {
    openal_buffer_resource_t *buffer_res = (openal_buffer_resource_t*)malloc(sizeof(openal_buffer_resource_t));
    if (!buffer_res) return NULL;

    buffer_res->buffer = buffer;
    Q_strncpyz(buffer_res->buffer_name, name, sizeof(buffer_res->buffer_name));

    return Resource_Register(RESOURCE_TYPE_OPENAL_BUFFER, buffer_res, Resource_CleanupOpenALBuffer, name);
}

/*
=============================================================================
Synchronization Resource Management
=============================================================================
*/

void Resource_CleanupMutex(void *data) {
    if (!data) return;

    mutex_resource_t *mutex_res = (mutex_resource_t*)data;

    // Mutex cleanup would go here (platform-specific)
    memset(mutex_res, 0, sizeof(mutex_resource_t));
    free(mutex_res);
}

resource_handle_t* Resource_CreateMutex(void *mutex_handle, const char *name) {
    mutex_resource_t *mutex_res = (mutex_resource_t*)malloc(sizeof(mutex_resource_t));
    if (!mutex_res) return NULL;

    mutex_res->mutex_handle = mutex_handle;
    Q_strncpyz(mutex_res->mutex_name, name, sizeof(mutex_res->mutex_name));

    return Resource_Register(RESOURCE_TYPE_MUTEX, mutex_res, Resource_CleanupMutex, name);
}

void Resource_CleanupThread(void *data) {
    if (!data) return;

    thread_resource_t *thread_res = (thread_resource_t*)data;

    if (thread_res->join_on_cleanup && thread_res->thread_handle) {
        // Thread join would go here (platform-specific)
    }

    memset(thread_res, 0, sizeof(thread_resource_t));
    free(thread_res);
}

resource_handle_t* Resource_CreateThread(void *thread_handle, qboolean join_on_cleanup, const char *name) {
    thread_resource_t *thread_res = (thread_resource_t*)malloc(sizeof(thread_resource_t));
    if (!thread_res) return NULL;

    thread_res->thread_handle = thread_handle;
    thread_res->join_on_cleanup = join_on_cleanup;
    Q_strncpyz(thread_res->thread_name, name, sizeof(thread_res->thread_name));

    return Resource_Register(RESOURCE_TYPE_THREAD, thread_res, Resource_CleanupThread, name);
}

/*
=============================================================================
Custom Resource Management
=============================================================================
*/

resource_handle_t* Resource_CreateCustom(void *user_data, resource_cleanup_fn cleanup_fn, const char *name) {
    return Resource_Register(RESOURCE_TYPE_CUSTOM, user_data, cleanup_fn, name);
}

/*
=============================================================================
Scope Guards and Defer Mechanisms
=============================================================================
*/

void Resource_ExecuteScopeGuard(void *data) {
    if (!data) return;

    scope_guard_t *guard = (scope_guard_t*)data;
    if (!guard->executed && guard->guard_fn) {
        guard->guard_fn(guard->user_data);
        guard->executed = qtrue;
    }
}

/*
=============================================================================
Resource Leak Detection
=============================================================================
*/

uint32_t Resource_DetectLeaks(resource_leak_info_t **leaks) {
    static resource_leak_info_t leak_buffer[256];
    uint32_t leak_count = 0;

    // Check all active scopes for uncleared resources
    resource_scope_t *scope = resource_manager.current_scope;
    while (scope) {
        for (uint32_t i = 0; i < scope->resource_count && leak_count < 256; i++) {
            resource_handle_t *handle = scope->resources[i];
            if (handle && handle->auto_cleanup) {
                leak_buffer[leak_count].type = handle->type;
                leak_buffer[leak_count].resource_name = handle->resource_name;
                leak_buffer[leak_count].allocation_time = handle->allocation_time;
                leak_buffer[leak_count].allocation_scope = scope->scope_name;
                leak_count++;
            }
        }
        scope = scope->parent_scope;
    }

    if (leaks) {
        *leaks = leak_count > 0 ? leak_buffer : NULL;
    }

    return leak_count;
}

void Resource_ReportLeaks(void) {
    resource_leak_info_t *leaks = NULL;
    uint32_t leak_count = Resource_DetectLeaks(&leaks);

    if (leak_count == 0) {
        Com_Printf("No resource leaks detected\n");
        return;
    }

    Com_Printf("=== Resource Leak Report (%u leaks) ===\n", leak_count);

    for (uint32_t i = 0; i < leak_count; i++) {
        const resource_leak_info_t *leak = &leaks[i];
        uint64_t age_ms = Sys_Milliseconds() - leak->allocation_time;

        Com_Printf("Leak %u: %s resource '%s'\n",
                  i + 1, Resource_GetTypeString(leak->type), leak->resource_name);
        Com_Printf("  Allocated in scope: %s\n", leak->allocation_scope);
        Com_Printf("  Age: %.2f seconds\n", age_ms / 1000.0f);
        Com_Printf("\n");
    }

    Com_Printf("===================================\n");
}

/*
=============================================================================
Resource Statistics and Monitoring
=============================================================================
*/

void Resource_GetStatistics(resource_statistics_t *stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(resource_statistics_t));

    // These would be tracked with atomic counters in a full implementation
    stats->total_allocations = resource_manager.total_allocated_resources;
    stats->current_allocations = 0; // Would need to count active resources
    stats->peak_allocations = resource_manager.total_allocated_resources; // Simplified
}

void Resource_GetStatisticsByType(resource_type_t type, resource_statistics_t *stats) {
    // Implementation would track per-type statistics
    Resource_GetStatistics(stats);
}

void Resource_ResetStatistics(void) {
    // Reset all statistics counters
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* Resource_GetTypeString(resource_type_t type) {
    if (type >= RESOURCE_TYPE_COUNT) return "Unknown";
    return resource_type_strings[type];
}

qboolean Resource_IsValidHandle(const resource_handle_t *handle) {
    if (!handle) return qfalse;
    if (handle->type >= RESOURCE_TYPE_COUNT) return qfalse;
    return qtrue;
}

void Resource_PrintScopeTree(void) {
    Com_Printf("=== Resource Scope Tree ===\n");

    resource_scope_t *scope = resource_manager.current_scope;
    int depth = 0;

    while (scope) {
        for (int i = 0; i < depth; i++) Com_Printf("  ");
        Com_Printf("%s (%u resources)\n", scope->scope_name, scope->resource_count);
        scope = scope->parent_scope;
        depth++;
    }

    Com_Printf("===========================\n");
}

/*
=============================================================================
RAII-style Convenience Macros
=============================================================================
*/

// Safe resource usage macros
#define WITH_MEMORY(size, name) \
    resource_handle_t *name##_handle = Resource_AllocateMemory(size, #name); \
    void *name = name##_handle ? Resource_GetMemoryPtr(name##_handle) : NULL; \
    DEFER({ if (name##_handle) Resource_Cleanup(name##_handle); })

#define WITH_FILE(filename, mode, name) \
    resource_handle_t *name##_handle = Resource_OpenFile(filename, mode, #name); \
    FILE *name = name##_handle ? Resource_GetFileHandle(name##_handle) : NULL; \
    DEFER({ if (name##_handle) Resource_Cleanup(name##_handle); })

// Automatic cleanup on return/error
#define AUTO_CLEANUP __attribute__((cleanup(Resource_AutoCleanup)))

// Exception-safe resource management
#define RESOURCE_TRANSACTION_BEGIN() \
    resource_scope_t *transaction_scope = Resource_CreateScope("transaction"); \
    Resource_EnterScope(transaction_scope); \
    qboolean transaction_success = qfalse;

#define RESOURCE_TRANSACTION_COMMIT() \
    transaction_success = qtrue;

#define RESOURCE_TRANSACTION_END() \
    if (!transaction_success) { \
        /* Transaction failed, cleanup will happen automatically */ \
    } \
    Resource_ExitScope();

// Internal auto cleanup function for cleanup attribute
void Resource_AutoCleanup(void *ptr) {
    if (ptr) {
        // This would need more sophisticated implementation
        // For now, assume it's a resource handle
        Resource_Cleanup((resource_handle_t*)ptr);
    }
}
