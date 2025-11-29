# Filesystem Improvements Plan

## Overview
This document outlines potential improvements to the Quake 3 filesystem implementation to enhance performance, reliability, and maintainability.

## Current Limitations

1. **Sequential Path Searching**: Every file lookup searches through all search paths sequentially
2. **No File Resolution Caching**: Same files are resolved repeatedly without caching
3. **Limited File Handles**: Only 64 handles (`MAX_FILE_HANDLES = 64`)
4. **Synchronous I/O**: All file operations block
5. **No Memory-Mapped Files**: Large files loaded entirely into memory
6. **Case Sensitivity Issues**: Mentioned as TODO in comments
7. **No File Existence Cache**: `FS_FileExists` opens/closes files repeatedly
8. **Sequential PK3 Loading**: PK3 files loaded one at a time
9. **Limited Path Length**: `MAX_ZPATH = 256` may be restrictive
10. **No Prefetching**: No proactive file loading

---

## Proposed Improvements

### 1. File Path Resolution Cache (High Priority)

**Problem**: `FS_FOpenFileRead` searches through all search paths sequentially every time, even for frequently accessed files.

**Solution**: Implement a hash table cache that maps file paths to their resolved locations (pak file + index or directory path).

**Benefits**:
- Dramatically faster file lookups for frequently accessed files
- Reduces redundant path traversal
- Minimal memory overhead

**Implementation**:
```c
#define FS_PATH_CACHE_SIZE 1024

typedef struct {
    char path[MAX_ZPATH];
    searchpath_t *resolvedPath;
    pack_t *pak;
    fileInPack_t *pakFile;
    qboolean isPakFile;
    uint32_t hash;
} fs_path_cache_entry_t;

static fs_path_cache_entry_t fs_pathCache[FS_PATH_CACHE_SIZE];
static int fs_pathCacheCount = 0;
```

**Cvar**: `fs_pathCache` (default: 1, enable path caching)

---

### 2. File Existence Cache (High Priority)

**Problem**: `FS_FileExists` and `FS_SV_FileExists` open/close files repeatedly, causing unnecessary I/O.

**Solution**: Cache file existence results with TTL (time-to-live) based on file modification time.

**Benefits**:
- Eliminates redundant file system calls
- Faster existence checks
- Reduces I/O overhead

**Implementation**:
```c
typedef struct {
    char path[MAX_ZPATH];
    qboolean exists;
    fileTime_t lastCheck;
    uint32_t hash;
} fs_existence_cache_entry_t;
```

**Cvar**: `fs_existenceCache` (default: 1, enable existence caching)

---

### 3. Increased File Handle Limit (Medium Priority)

**Problem**: `MAX_FILE_HANDLES = 64` may be insufficient for modern games with many concurrent file operations.

**Solution**: Increase limit and implement dynamic allocation.

**Benefits**:
- Support for more concurrent file operations
- Better handling of complex mods
- Prevents handle exhaustion

**Implementation**:
- Increase `MAX_FILE_HANDLES` to 256 or 512
- Or implement dynamic handle allocation with a pool

**Cvar**: `fs_maxHandles` (default: 256, max concurrent file handles)

---

### 4. Case-Insensitive File Lookups (Medium Priority)

**Problem**: Case sensitivity issues mentioned in TODO comments, especially on case-insensitive filesystems (Windows, macOS).

**Solution**: Normalize all file paths to lowercase for comparison while preserving original case for display.

**Benefits**:
- Cross-platform compatibility
- Prevents case-sensitivity bugs
- Better mod compatibility

**Implementation**:
- Add `FS_NormalizePath()` function
- Store normalized paths in cache
- Compare normalized paths during lookup

---

### 5. Memory-Mapped File Support (Medium Priority)

**Problem**: Large files (maps, models) are loaded entirely into memory, wasting RAM and causing load spikes.

**Solution**: Use memory-mapped files (`mmap` on Unix, `CreateFileMapping` on Windows) for large read-only files.

**Benefits**:
- Reduced memory usage
- Faster loading (OS handles paging)
- Better for large files

**Implementation**:
- Add `FS_MapFile()` / `FS_UnmapFile()` functions
- Use for files > 1MB
- Fallback to regular read for small files

**Cvar**: `fs_mmapFiles` (default: 1, enable memory-mapped files)
**Cvar**: `fs_mmapThreshold` (default: 1048576, minimum size for mmap in bytes)

---

### 6. Async File I/O (Low Priority)

**Problem**: All file operations are synchronous, blocking the main thread.

**Solution**: Implement async I/O using threads or platform-specific async APIs.

**Benefits**:
- Non-blocking file operations
- Better frame rate stability
- Parallel file loading

**Implementation**:
- Add `FS_ReadAsync()` / `FS_WriteAsync()` functions
- Use thread pool for I/O operations
- Callback-based completion

**Cvar**: `fs_asyncIO` (default: 0, enable async I/O)
**Cvar**: `fs_asyncThreads` (default: 2, number of async I/O threads)

---

### 7. Parallel PK3 Loading (Medium Priority)

**Problem**: PK3 files are loaded sequentially during startup, causing long load times.

**Solution**: Load multiple PK3 files in parallel using threads.

**Benefits**:
- Faster startup times
- Better CPU utilization
- Improved user experience

**Implementation**:
- Thread pool for PK3 loading
- Load PK3s in batches
- Maintain search order

**Cvar**: `fs_parallelPk3Load` (default: 1, enable parallel PK3 loading)
**Cvar**: `fs_pk3LoadThreads` (default: 4, number of PK3 loading threads)

---

### 8. File Prefetching (Low Priority)

**Problem**: Files are loaded on-demand, causing stuttering when loading assets during gameplay.

**Solution**: Prefetch likely-to-be-needed files based on game state.

**Benefits**:
- Smoother gameplay
- Reduced loading stutters
- Better user experience

**Implementation**:
- Track file access patterns
- Prefetch files likely to be needed
- Background loading thread

**Cvar**: `fs_prefetch` (default: 0, enable file prefetching)
**Cvar**: `fs_prefetchDistance` (default: 1, prefetch files N levels ahead)

---

### 9. Improved Error Messages (Low Priority)

**Problem**: File errors are often cryptic and don't provide enough context.

**Solution**: Enhanced error messages with file paths, search paths attempted, and suggestions.

**Benefits**:
- Easier debugging
- Better user experience
- Faster problem resolution

**Implementation**:
- Track search paths attempted
- Include full path information
- Suggest common fixes

---

### 10. Path Normalization Optimization (Low Priority)

**Problem**: Path building and normalization happens repeatedly for the same paths.

**Solution**: Cache normalized paths and reuse them.

**Benefits**:
- Reduced string operations
- Faster path building
- Lower CPU usage

**Implementation**:
- Cache normalized paths
- Reuse cached paths
- Invalidate on filesystem changes

---

### 11. File Handle Pooling (Medium Priority)

**Problem**: File handles are allocated/deallocated frequently, causing fragmentation.

**Solution**: Implement a handle pool with reuse.

**Benefits**:
- Reduced allocation overhead
- Better memory locality
- Faster handle allocation

**Implementation**:
- Pre-allocate handle pool
- Reuse handles
- Track handle usage

---

### 12. Extended Path Length Support (Low Priority)

**Problem**: `MAX_ZPATH = 256` may be restrictive for deep directory structures.

**Solution**: Increase limit or use dynamic allocation.

**Benefits**:
- Support for deeper directory structures
- Better compatibility with modern filesystems
- More flexible mod support

**Implementation**:
- Increase `MAX_ZPATH` to 512 or 1024
- Or use dynamic string allocation

**Cvar**: `fs_maxPathLength` (default: 512, maximum path length)

---

## Implementation Priority

### Phase 1 (High Impact, Low Risk)
1. File Path Resolution Cache
2. File Existence Cache
3. Increased File Handle Limit

### Phase 2 (Medium Impact, Medium Risk)
4. Case-Insensitive File Lookups
5. Memory-Mapped File Support
6. Parallel PK3 Loading
7. File Handle Pooling

### Phase 3 (Lower Impact, Higher Risk)
8. Async File I/O
9. File Prefetching
10. Improved Error Messages
11. Path Normalization Optimization
12. Extended Path Length Support

---

## Performance Metrics

Track these metrics to measure improvement:
- Average file lookup time
- File handle usage
- Memory usage
- Startup time
- I/O wait time
- Cache hit rate

---

## Testing Considerations

1. **Cache Invalidation**: Ensure caches are properly invalidated on filesystem changes
2. **Thread Safety**: Ensure thread-safe implementations for parallel operations
3. **Memory Leaks**: Verify no memory leaks in cache implementations
4. **Cross-Platform**: Test on Windows, Linux, macOS
5. **Mod Compatibility**: Ensure improvements don't break existing mods
6. **Performance**: Benchmark before/after improvements

---

## Notes

- All improvements should be optional via cvars
- Maintain backward compatibility
- Consider memory usage vs. performance tradeoffs
- Document new cvars and behaviors
- Add debug output for troubleshooting

