# Virtual Filesystem v2 (VFS v2)

## Overview

Virtual Filesystem v2 is a modern, priority-based mount table system that replaces the legacy search path approach with a more flexible and secure architecture. It provides advanced features like sandboxing, caching, file monitoring, and priority-based file resolution.

## Architecture

### Mount Table
VFS v2 uses a priority-ordered mount table instead of the legacy linked list of search paths:

```
High Priority → [System Files] → [Mods] → [Base Game] → [CD/Read-only] → [Fallback] → Low Priority
```

### Mount Types
- **PAK Mounts**: `.pk3`, `.orb` files with compression and integrity checking
- **Directory Mounts**: Direct filesystem access with optional sandboxing
- **Virtual Mounts**: Future support for network filesystems, archives, etc.

### Priority System
Mounts are searched in priority order (higher numbers = higher priority):

```c
enum fsMountPriority_t {
    FS_PRIORITY_SYSTEM    = 1000,  // System files (highest)
    FS_PRIORITY_MOD       = 800,   // User modifications
    FS_PRIORITY_GAME      = 600,   // Base game content
    FS_PRIORITY_CD        = 400,   // CD/read-only content
    FS_PRIORITY_FALLBACK  = 200    // Fallback locations (lowest)
};
```

## Features

### Sandboxing System
VFS v2 includes comprehensive sandboxing to prevent malicious mod behavior:

```c
typedef struct {
    qboolean allowExecutables;    // Block .exe, .so, .dll files
    qboolean allowConfig;         // Block .cfg files
    qboolean allowSaves;          // Block save file access
    char allowedPaths[16][MAX_QPATH];  // Whitelist allowed directories
    int numAllowedPaths;
} fsSandboxRules_t;
```

### File Caching
Built-in file caching system for performance optimization:

- **LRU Eviction**: Least Recently Used cache replacement
- **Size Limits**: Configurable cache size and entry limits
- **Compression**: Optional data compression
- **Statistics**: Hit rates and performance metrics

### File Monitoring
Hot-reload support with file change detection:

```c
typedef enum {
    FS_CHANGE_ADDED,
    FS_CHANGE_MODIFIED,
    FS_CHANGE_DELETED,
    FS_CHANGE_RENAMED
} fsChangeType_t;
```

### Write Policies
Flexible write access control:

```c
typedef enum {
    FS_WRITE_DENY,     // No writes allowed
    FS_WRITE_ALLOW,    // Writes allowed
    FS_WRITE_SANDBOX   // Writes allowed but sandboxed
} fsWritePolicy_t;
```

## Console Commands

### Mount Management
```bash
# List all mounted filesystems
fs_mount_list

# Add a new mount
fs_mount_add <mount_point> <path> <priority>

# Remove a mount
fs_mount_remove <mount_point>

# Enable/disable mounts
fs_mount_enable <mount_point>
fs_mount_disable <mount_point>

# Set write mount location
fs_mount_write <mount_point>
```

### Statistics and Debugging
```bash
# Show mount statistics
fs_mount_stats

# Show cache performance
fs_cache_stats

# Show sandbox status
fs_sandbox_status
```

## Usage Examples

### Basic Mod Mounting
```c
// Mount a mod directory with mod priority
FS_Mod_Mount("mymod", "/path/to/mod", FS_PRIORITY_MOD);

// Mount a PAK file
FS_Mod_Mount("texturepack.pk3", "/path/to/pack.pk3", FS_PRIORITY_GAME);
```

### Sandbox Configuration
```c
// Initialize default sandbox rules (allows config, saves, blocks executables)
fsSandboxRules_t rules;
FS_Sandbox_InitDefaultRules(&rules);

// Initialize restrictive mod rules
FS_Sandbox_InitModRules(&rules);  // Blocks config, saves, executables

// Validate file operation
if (FS_Sandbox_ValidateOperation("config/malicious.cfg", mount, qtrue)) {
    // Operation allowed
} else {
    // Operation blocked by sandbox
}
```

### File Monitoring
```c
// Register callback for file changes
void OnFileChange(const fsChangeEvent_t *event) {
    if (event->type == FS_CHANGE_MODIFIED) {
        Com_Printf("File modified: %s\n", event->oldPath);
        // Trigger shader reload, etc.
    }
}

FS_Monitor_RegisterCallback(OnFileChange);
FS_Monitor_AddPath("shaders/");
```

### Cache Management
```c
// Initialize cache with 100MB limit
fsCacheConfig_t config = {
    .enabled = qtrue,
    .maxSizeBytes = 100 * 1024 * 1024,
    .maxEntries = 1000,
    .compressData = qtrue
};
FS_Cache_Init(&config);

// Use cached file access
void *data;
size_t size;
if (FS_Cache_Lookup("models/player.md3", &data, &size)) {
    // Use cached data
} else {
    // Load and cache file
    FS_Cache_Store("models/player.md3", fileData, fileSize);
}
```

## Migration from Legacy Filesystem

### Automatic Migration
VFS v2 automatically migrates legacy search paths:

```c
// Automatically called during filesystem initialization
FS_MigrateLegacySearchPaths();
```

### Compatibility Layer
Legacy functions continue to work through compatibility shims:

```c
// Legacy API (still works)
int FS_FOpenFileRead(const char *qpath, fileHandle_t *file, qboolean uniqueFILE);

// Maps to new VFS v2 API internally
int FS_Mount_FindFile(const char *qpath, fileHandle_t *file,
                     fsMount_t **outMount, pack_t **outPak,
                     fileInPack_t **outPakFile);
```

## Performance Benefits

### Priority-Based Resolution
- **O(1) mount lookup** with priority arrays
- **Early termination** when high-priority files are found
- **No linear search** through all search paths

### Caching System
- **Reduced I/O** through intelligent caching
- **Configurable policies** for different use cases
- **Memory management** with size limits and eviction

### Statistics Tracking
- **Access counting** for performance analysis
- **Hit rate monitoring** for cache efficiency
- **Mount utilization** tracking

## Security Features

### Sandboxing
- **Path restrictions** prevent directory traversal
- **Executable blocking** stops malware execution
- **Config isolation** protects user settings
- **Save protection** prevents save file corruption

### Validation
- **Path sanitization** prevents invalid paths
- **Permission checking** enforces access controls
- **Checksum verification** for PAK integrity
- **Size limits** prevent resource exhaustion

## Integration Status

### Current State
- ✅ **Mount table implementation** - Complete
- ✅ **Priority system** - Complete
- ✅ **Sandboxing** - Complete
- ✅ **File caching** - Complete
- ✅ **File monitoring** - Complete
- ✅ **Console commands** - Complete
- ✅ **Legacy compatibility** - Complete
- ✅ **Unit tests** - Complete

### Renderer Integration
- ✅ **OpenGL renderer** - Integrated
- ✅ **Vulkan renderer** - Integrated
- ✅ **Mod support** - Full compatibility
- ✅ **PAK file support** - Maintained

## Future Enhancements

### Planned Features
- **Network mounts** - Remote filesystem access
- **Archive compression** - Additional compression formats
- **Memory mapping** - Zero-copy file access
- **Asynchronous I/O** - Non-blocking file operations
- **Encryption** - File encryption/decryption
- **Deduplication** - Automatic file deduplication

### Research Areas
- **Cloud storage** - AWS S3, Google Cloud integration
- **CDN support** - Content delivery network integration
- **Peer-to-peer** - Distributed file sharing
- **Versioning** - File version management
- **Backup/restore** - Automated filesystem backup

## Configuration

### CVARs
```c
// Enable VFS v2 (default: enabled)
set fs_use_vfs_v2 "1"

// Cache settings
set fs_cache_enabled "1"
set fs_cache_size_mb "100"
set fs_cache_compress "1"

// Sandbox settings
set fs_sandbox_enabled "1"
set fs_sandbox_allow_config "1"
set fs_sandbox_allow_saves "1"
```

### Environment Variables
```bash
# Force legacy filesystem (for compatibility testing)
export FS_USE_VFS_V2=0

# Custom cache directory
export FS_CACHE_DIR="/tmp/idtech3_cache"
```

## Troubleshooting

### Common Issues
- **Mount conflicts**: Use different mount points for overlapping paths
- **Permission errors**: Check sandbox rules and file permissions
- **Cache corruption**: Clear cache with `FS_Cache_Clear()`
- **Performance issues**: Monitor with `fs_mount_stats` and `fs_cache_stats`

### Debug Commands
```bash
// Enable verbose filesystem logging
developer 1
set fs_debug "1"

// Show mount table dumps
fs_mount_list
fs_mount_stats

// Clear caches
fs_cache_clear
```

## References

- **Design Document**: `docs/VIRTUAL_FS_V2.md`
- **API Reference**: `src/common/files_v2.h`
- **Implementation**: `src/common/files_v2.c`
- **Unit Tests**: `src/tests/test_filesystem_v2.c`
- **Integration**: `src/common/files.c`

---

*Virtual Filesystem v2 provides a modern, secure, and high-performance foundation for idTech3++'s asset management system.*