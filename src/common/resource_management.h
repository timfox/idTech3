/*
=============================================================================
Resource Management Framework

RAII patterns and automatic resource cleanup for C.
=============================================================================
*/

#ifndef __RESOURCE_MANAGEMENT_H__
#define __RESOURCE_MANAGEMENT_H__

#include "q_shared.h"
#include <setjmp.h>

// Resource types for automatic management
typedef enum {
    RESOURCE_TYPE_MEMORY,
    RESOURCE_TYPE_FILE,
    RESOURCE_TYPE_SOCKET,
    RESOURCE_TYPE_MUTEX,
    RESOURCE_TYPE_THREAD,
    RESOURCE_TYPE_VULKAN_BUFFER,
    RESOURCE_TYPE_VULKAN_IMAGE,
    RESOURCE_TYPE_VULKAN_SHADER,
    RESOURCE_TYPE_OPENAL_SOURCE,
    RESOURCE_TYPE_OPENAL_BUFFER,
    RESOURCE_TYPE_CUSTOM,
    RESOURCE_TYPE_COUNT
} resource_type_t;

// Resource cleanup function signature
typedef void (*resource_cleanup_fn)(void *resource_data);

// Resource handle with automatic cleanup
typedef struct resource_handle {
    resource_type_t type;
    void *resource_data;
    resource_cleanup_fn cleanup_fn;
    const char *resource_name;
    qboolean auto_cleanup;
    uint64_t allocation_time;
    struct resource_handle *next; // For linked list in scopes
} resource_handle_t;

// Scope-based resource management
typedef struct resource_scope {
    const char *scope_name;
    resource_handle_t *resources;
    uint32_t resource_count;
    uint32_t max_resources;
    qboolean auto_cleanup_on_exit;
    struct resource_scope *parent_scope;
    jmp_buf cleanup_context; // For exception-safe cleanup
} resource_scope_t;

// Resource manager global state
typedef struct {
    resource_scope_t *current_scope;
    resource_scope_t *global_scope;
    uint32_t total_allocated_resources;
    uint32_t active_scopes;
    qboolean auto_cleanup_enabled;
    qboolean leak_detection_enabled;
} resource_manager_t;

extern resource_manager_t resource_manager;

// Resource Management API
qboolean Resource_Init(void);
void Resource_Shutdown(void);

// Scope Management
resource_scope_t* Resource_CreateScope(const char *scope_name);
void Resource_EnterScope(resource_scope_t *scope);
void Resource_ExitScope(void);
void Resource_ExitScopeTo(resource_scope_t *target_scope);

// Resource Registration and Management
resource_handle_t* Resource_Register(resource_type_t type, void *resource_data,
                                   resource_cleanup_fn cleanup_fn, const char *name);
void Resource_Unregister(resource_handle_t *handle);
void Resource_Cleanup(resource_handle_t *handle);
void Resource_CleanupAllInScope(resource_scope_t *scope);

// RAII-style Resource Wrappers
#define RESOURCE_WRAP(type, resource, cleanup_fn, name) \
    Resource_Register(type, resource, cleanup_fn, name)

// Memory Resource Management
typedef struct {
    void *ptr;
    size_t size;
    qboolean zero_on_free;
} memory_resource_t;

resource_handle_t* Resource_AllocateMemory(size_t size, const char *name);
resource_handle_t* Resource_AllocateMemoryZero(size_t size, const char *name);
void* Resource_GetMemoryPtr(resource_handle_t *handle);

// File Resource Management
typedef struct {
    FILE *file;
    char filename[256];
    qboolean close_on_cleanup;
} file_resource_t;

resource_handle_t* Resource_OpenFile(const char *filename, const char *mode, const char *resource_name);
FILE* Resource_GetFileHandle(resource_handle_t *handle);

// Network Resource Management
typedef struct {
    int socket_fd;
    char address[64];
    uint16_t port;
} socket_resource_t;

resource_handle_t* Resource_CreateSocket(int socket_fd, const char *address, uint16_t port, const char *name);

// Vulkan Resource Management
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
} vulkan_buffer_resource_t;

typedef struct {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    VkImageLayout layout;
} vulkan_image_resource_t;

typedef struct {
    VkShaderModule module;
    char shader_name[128];
} vulkan_shader_resource_t;

resource_handle_t* Resource_CreateVulkanBuffer(VkBuffer buffer, VkDeviceMemory memory,
                                             VkDeviceSize size, VkBufferUsageFlags usage,
                                             const char *name);
resource_handle_t* Resource_CreateVulkanImage(VkImage image, VkImageView view,
                                            VkDeviceMemory memory, const char *name);
resource_handle_t* Resource_CreateVulkanShader(VkShaderModule module, const char *shader_name);

// OpenAL Resource Management
typedef struct {
    ALuint source;
    char source_name[64];
} openal_source_resource_t;

typedef struct {
    ALuint buffer;
    char buffer_name[64];
} openal_buffer_resource_t;

resource_handle_t* Resource_CreateOpenALSource(ALuint source, const char *name);
resource_handle_t* Resource_CreateOpenALBuffer(ALuint buffer, const char *name);

// Synchronization Resource Management
typedef struct {
    void *mutex_handle; // Platform-specific mutex handle
    char mutex_name[64];
} mutex_resource_t;

typedef struct {
    void *thread_handle; // Platform-specific thread handle
    char thread_name[64];
    qboolean join_on_cleanup;
} thread_resource_t;

resource_handle_t* Resource_CreateMutex(void *mutex_handle, const char *name);
resource_handle_t* Resource_CreateThread(void *thread_handle, qboolean join_on_cleanup, const char *name);

// Custom Resource Management
typedef struct {
    void *user_data;
    void (*custom_cleanup)(void *user_data);
} custom_resource_t;

resource_handle_t* Resource_CreateCustom(void *user_data, resource_cleanup_fn cleanup_fn, const char *name);

// Scope Guards and Defer Mechanisms
typedef void (*scope_guard_fn)(void *user_data);

typedef struct {
    scope_guard_fn guard_fn;
    void *user_data;
    qboolean executed;
} scope_guard_t;

// Defer mechanism - executes cleanup when scope exits
#define DEFER(code) \
    do { \
        static qboolean defer_initialized = qfalse; \
        static scope_guard_t defer_guard; \
        if (!defer_initialized) { \
            defer_guard.guard_fn = [](void *data) { code; }; \
            defer_guard.user_data = NULL; \
            defer_guard.executed = qfalse; \
            Resource_Register(RESOURCE_TYPE_CUSTOM, &defer_guard, \
                            Resource_ExecuteScopeGuard, "defer_guard"); \
            defer_initialized = qtrue; \
        } \
    } while(0)

// RAII-style macros for common resources
#define RAII_MEMORY(size, name) \
    Resource_AllocateMemory(size, name)

#define RAII_FILE(filename, mode, name) \
    Resource_OpenFile(filename, mode, name)

#define RAII_SCOPE(name) \
    Resource_EnterScope(Resource_CreateScope(name))

#define RAII_SCOPE_EXIT() \
    Resource_ExitScope()

// Resource leak detection
typedef struct {
    resource_type_t type;
    const char *resource_name;
    uint64_t allocation_time;
    const char *allocation_scope;
} resource_leak_info_t;

uint32_t Resource_DetectLeaks(resource_leak_info_t **leaks);
void Resource_ReportLeaks(void);

// Resource statistics and monitoring
typedef struct {
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t current_allocations;
    uint64_t peak_allocations;
    uint64_t allocation_failures;
    uint64_t cleanup_failures;
} resource_statistics_t;

void Resource_GetStatistics(resource_statistics_t *stats);
void Resource_GetStatisticsByType(resource_type_t type, resource_statistics_t *stats);
void Resource_ResetStatistics(void);

// Utility functions
const char* Resource_GetTypeString(resource_type_t type);
qboolean Resource_IsValidHandle(const resource_handle_t *handle);
void Resource_PrintScopeTree(void);

// Internal cleanup functions
void Resource_ExecuteScopeGuard(void *data);
void Resource_CleanupMemory(void *data);
void Resource_CleanupFile(void *data);
void Resource_CleanupSocket(void *data);
void Resource_CleanupMutex(void *data);
void Resource_CleanupThread(void *data);
void Resource_CleanupVulkanBuffer(void *data);
void Resource_CleanupVulkanImage(void *data);
void Resource_CleanupVulkanShader(void *data);
void Resource_CleanupOpenALSource(void *data);
void Resource_CleanupOpenALBuffer(void *data);

#endif // __RESOURCE_MANAGEMENT_H__
