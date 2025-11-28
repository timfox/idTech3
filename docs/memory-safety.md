# Memory Safety and Profiling

## Overview

The id Tech 3 engine now includes comprehensive memory safety tools and profiling capabilities to help detect memory errors, leaks, and performance issues.

## AddressSanitizer (ASan)

AddressSanitizer detects memory errors such as:
- Use-after-free
- Heap buffer overflow
- Stack buffer overflow
- Use-after-return
- Memory leaks

### Usage

```bash
cd build
cmake .. -DENABLE_ASAN=ON
make
./idtech3.x86_64
```

### Output

ASan will print detailed error reports when memory errors are detected:
```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000f1d0
    #0 0x7f8b2c3d4f2a in main /path/to/file.c:123
    #1 0x7f8b2c1d82bf in __libc_start_main
```

## UndefinedBehaviorSanitizer (UBSan)

UndefinedBehaviorSanitizer detects undefined behavior such as:
- Integer overflow
- Null pointer dereference
- Invalid shift operations
- Out-of-bounds array access

### Usage

```bash
cd build
cmake .. -DENABLE_UBSAN=ON
make
./idtech3.x86_64
```

### Output

UBSan will print warnings when undefined behavior is detected:
```
file.c:123:5: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

## Memory Tracking System

The memory tracking system provides detailed statistics and leak detection for all memory allocations.

### Features

- **Per-type tracking**: Track memory by type (HUNK, ZONE, TEMP, SOUND, RENDERER, etc.)
- **Leak detection**: Automatically detect memory leaks on shutdown
- **Statistics**: Track allocated, freed, current, and peak memory usage
- **Detailed reports**: Generate detailed leak reports with file/line information

### CVars

| CVar | Default | Description |
|------|---------|-------------|
| `memtrack_enable` | `1` | Enable memory tracking |
| `memtrack_report_leaks` | `0` | Report leaks on shutdown |
| `memtrack_log_leaks` | `0` | Log leaks to memleaks.log |

### Usage

```c
#include "q_memtrack.h"

// Track allocation
void *ptr = Q_MemTrack_Malloc(1024, MEMTYPE_RENDERER);

// Track reallocation
ptr = Q_MemTrack_Realloc(ptr, 2048, MEMTYPE_RENDERER);

// Track free
Q_MemTrack_Free(ptr);

// Get statistics
memstats_t stats;
Q_MemTrack_GetStats(MEMTYPE_RENDERER, &stats);
Com_Printf("Renderer memory: %lld bytes allocated, %lld bytes current\n",
    (long long)stats.allocated, (long long)stats.current);

// Report leaks
Q_MemTrack_ReportLeaks();
```

### Memory Types

- `MEMTYPE_HUNK`: Hunk allocator memory
- `MEMTYPE_ZONE`: Zone allocator memory
- `MEMTYPE_TEMP`: Temporary allocations
- `MEMTYPE_SOUND`: Sound system memory
- `MEMTYPE_RENDERER`: Renderer memory
- `MEMTYPE_NETWORK`: Network buffers
- `MEMTYPE_FILESYSTEM`: File system buffers
- `MEMTYPE_SCRIPT`: Scripting memory
- `MEMTYPE_BOTLIB`: Bot library memory
- `MEMTYPE_OTHER`: Other allocations

## Valgrind Integration

Valgrind is a powerful memory debugging tool for Linux. The engine includes hooks for Valgrind integration.

### Usage

```bash
# Build with Valgrind support
cd build
cmake .. -DENABLE_VALGRIND=ON
make

# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./idtech3.x86_64
```

### Valgrind Options

- `--leak-check=full`: Full leak checking
- `--show-leak-kinds=all`: Show all leak kinds
- `--track-origins=yes`: Track origins of uninitialized values
- `--suppressions=valgrind.supp`: Use suppression file

### Suppression File

Create `valgrind.supp` to suppress known false positives:
```
{
   ignore_libc_malloc
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   fun:malloc
}
```

## Dr. Memory Integration (Windows)

Dr. Memory is a memory debugging tool for Windows similar to Valgrind.

### Usage

```bash
# Build with Dr. Memory support
cd build
cmake .. -DENABLE_DRMEMORY=ON
make

# Run with Dr. Memory
drmemory.exe idtech3.x86_64.exe
```

## Memory Statistics

The engine tracks comprehensive memory statistics:

### Per-Type Statistics

- **allocated**: Total bytes allocated
- **freed**: Total bytes freed
- **current**: Current bytes in use
- **peak**: Peak bytes in use
- **count**: Number of allocations
- **free_count**: Number of frees
- **leak_count**: Number of leaks detected

### Total Statistics

Aggregate statistics across all memory types.

## Best Practices

1. **Enable tracking in development**: Use `ENABLE_MEMORY_TRACKING=ON` during development
2. **Check for leaks regularly**: Set `memtrack_report_leaks 1` to catch leaks early
3. **Use appropriate memory types**: Choose the correct memory type for better tracking
4. **Run with sanitizers**: Use ASan/UBSan in CI/CD pipelines
5. **Profile regularly**: Check memory statistics to identify memory hotspots

## Performance Impact

- **Memory Tracking**: ~5-10% overhead, mainly from tracking overhead
- **ASan**: ~2x slowdown, significant memory overhead
- **UBSan**: ~1.5x slowdown, minimal memory overhead
- **Valgrind**: ~10-50x slowdown, use only for debugging

## Troubleshooting

### False Positives

Some memory may appear as leaks but is intentionally kept:
- Static buffers
- Cached data
- Global state

Use suppression files or adjust tracking settings.

### Performance Issues

If tracking causes performance issues:
- Disable in release builds
- Use sampling instead of full tracking
- Focus on specific memory types

### Build Issues

If sanitizers fail to build:
- Ensure compiler supports sanitizers (GCC 4.8+, Clang 3.1+)
- Check for conflicting flags
- Verify system libraries are compatible

## Example Workflow

```bash
# 1. Build with memory tracking
cmake .. -DENABLE_MEMORY_TRACKING=ON
make

# 2. Run and check for leaks
./idtech3.x86_64
# Check console for leak reports

# 3. Enable leak logging
/set memtrack_report_leaks 1
/set memtrack_log_leaks 1

# 4. Run with ASan for error detection
cmake .. -DENABLE_ASAN=ON
make
./idtech3.x86_64

# 5. Run with Valgrind for detailed analysis
valgrind --leak-check=full ./idtech3.x86_64
```

## Integration with CI/CD

Add to your CI pipeline:

```yaml
- name: Build with sanitizers
  run: |
    cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
    make
    
- name: Run tests
  run: |
    ./idtech3.x86_64 +set memtrack_report_leaks 1
```

