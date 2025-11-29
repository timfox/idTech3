# Additional Filesystem Improvements

Based on analysis of the current filesystem implementation, here are additional improvements we can make:

## High Priority Improvements

### 1. Case-Insensitive File Lookups
**Status**: Mentioned as TODO in code comments
**Problem**: File lookups are case-sensitive, causing issues on case-insensitive filesystems (Windows, macOS)
**Solution**: Add case-insensitive comparison option with caching
**Benefits**:
- Better cross-platform compatibility
- Fewer file-not-found errors
- Improved mod compatibility

**Implementation**:
- Add `fs_caseInsensitive` cvar (default: 1 on Windows/macOS, 0 on Linux)
- Use case-insensitive hash function for cache
- Normalize filenames to lowercase for comparison

### 2. Path Normalization Caching
**Problem**: `FS_BuildOSPath` is called repeatedly for the same paths
**Solution**: Cache normalized paths to reduce string operations
**Benefits**:
- Reduced CPU overhead
- Faster path building
- Lower memory allocations

**Implementation**:
```c
typedef struct {
    char normalized[MAX_OSPATH*2+1];
    uint32_t hash;
    qboolean valid;
} fs_normalized_path_cache_t;

static fs_normalized_path_cache_t fs_pathNormalizeCache[256];
```

### 3. Better Error Messages with Context
**Problem**: File errors don't provide enough context for debugging
**Solution**: Enhanced error messages with search paths attempted
**Benefits**:
- Easier debugging
- Better user experience
- Faster problem resolution

**Implementation**:
- Track which search paths were checked
- Include full path information in errors
- Suggest common fixes (file not found, wrong gamedir, etc.)

### 4. File Handle Pooling
**Problem**: File handles allocated/deallocated frequently
**Solution**: Pre-allocate handle pool with reuse
**Benefits**:
- Reduced allocation overhead
- Better memory locality
- Faster handle allocation

**Implementation**:
- Pre-allocate handle pool at startup
- Track handle usage statistics
- Reuse handles efficiently

## Medium Priority Improvements

### 5. Memory-Mapped File Support
**Problem**: Large files loaded entirely into memory
**Solution**: Use memory-mapped files for files > threshold
**Benefits**:
- Lower memory usage
- Faster loading for large files
- Better OS caching

**Implementation**:
- Add `FS_MapFile()` / `FS_UnmapFile()` functions
- Use for files > 1MB
- Fallback to regular read for small files

**Cvars**:
- `fs_mmapFiles` (default: 1)
- `fs_mmapThreshold` (default: 1048576 bytes)

### 6. Cache Invalidation on Filesystem Changes
**Problem**: Cache doesn't invalidate when filesystem changes
**Solution**: Track filesystem modification times and invalidate stale cache entries
**Benefits**:
- Always accurate cache
- No stale data issues
- Better reliability

**Implementation**:
- Store file modification times in cache
- Check modification times on cache hits
- Invalidate entries when files change

### 7. Parallel PK3 Loading
**Problem**: PK3 files loaded sequentially during startup
**Solution**: Load multiple PK3 files in parallel using threads
**Benefits**:
- Faster startup times
- Better CPU utilization
- Improved user experience

**Implementation**:
- Thread pool for PK3 loading
- Load PK3s in batches
- Maintain search order

**Cvars**:
- `fs_parallelPk3Load` (default: 1)
- `fs_pk3LoadThreads` (default: 4)

### 8. File Access Statistics
**Problem**: No visibility into file access patterns
**Solution**: Track file access statistics
**Benefits**:
- Identify hot files
- Optimize cache sizes
- Better performance tuning

**Implementation**:
- Track access counts per file
- Track access patterns
- Provide command to view statistics

**Command**: `fs_fileStats` - Show file access statistics

## Low Priority Improvements

### 9. Async File I/O
**Problem**: All file operations are synchronous
**Solution**: Implement async I/O using threads
**Benefits**:
- Non-blocking file operations
- Better frame rate stability
- Parallel file loading

**Implementation**:
- Add `FS_ReadAsync()` / `FS_WriteAsync()` functions
- Use thread pool for I/O operations
- Callback-based completion

**Cvars**:
- `fs_asyncIO` (default: 0)
- `fs_asyncThreads` (default: 2)

### 10. File Prefetching
**Problem**: Files loaded on-demand, causing stuttering
**Solution**: Prefetch likely-to-be-needed files
**Benefits**:
- Smoother gameplay
- Reduced loading stutters
- Better user experience

**Implementation**:
- Track file access patterns
- Prefetch files likely to be needed
- Background loading thread

**Cvars**:
- `fs_prefetch` (default: 0)
- `fs_prefetchDistance` (default: 1)

### 11. Extended Path Length Support
**Problem**: `MAX_ZPATH = 256` may be restrictive
**Solution**: Increase limit or use dynamic allocation
**Benefits**:
- Support for deeper directory structures
- Better compatibility with modern filesystems
- More flexible mod support

**Implementation**:
- Increase `MAX_ZPATH` to 512 or 1024
- Or use dynamic string allocation

**Cvar**: `fs_maxPathLength` (default: 512)

### 12. Better Buffer Overflow Protection
**Problem**: Some string operations may overflow buffers
**Solution**: Add bounds checking and use safer string functions
**Benefits**:
- Prevent crashes
- Better security
- More robust code

**Implementation**:
- Use `Q_strncpyz` consistently
- Add bounds checking
- Use `strncat` instead of `strcat`

## C23-Specific Improvements

### 13. More C23 Features
**Opportunities**:
- Type-generic macros with `_Generic`
- `if consteval` for compile-time optimizations
- Better string handling with bounds checking
- `[[deprecated]]` attributes for legacy functions
- More `constexpr` optimizations

### 14. Better Type Safety
**Opportunities**:
- Use `typeof` for better type inference
- Stronger typing for file handles
- Better const correctness
- Type-safe wrappers

## Performance Optimizations

### 15. Reduce Redundant Operations
**Opportunities**:
- Cache hash calculations
- Reuse path buffers
- Reduce string allocations
- Optimize hot paths

### 16. Better Cache Management
**Opportunities**:
- LRU eviction policy
- Adaptive cache sizing
- Cache warming on startup
- Better cache hit prediction

## Implementation Priority

### Phase 1 (High Impact, Low Risk)
1. Case-Insensitive File Lookups
2. Path Normalization Caching
3. Better Error Messages
4. File Handle Pooling

### Phase 2 (Medium Impact, Medium Risk)
5. Memory-Mapped File Support
6. Cache Invalidation
7. Parallel PK3 Loading
8. File Access Statistics

### Phase 3 (Lower Impact, Higher Risk)
9. Async File I/O
10. File Prefetching
11. Extended Path Length
12. Buffer Overflow Protection

