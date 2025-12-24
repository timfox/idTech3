# Performance Tuning Guide

## Overview

This guide provides comprehensive information on optimizing id Tech 3 engine performance, including profiling tools, optimization techniques, CVar tuning, and best practices for developers and end users.

## Table of Contents

1. [Performance Profiling](#performance-profiling)
2. [CPU Optimization](#cpu-optimization)
3. [GPU Optimization](#gpu-optimization)
4. [Memory Optimization](#memory-optimization)
5. [Network Optimization](#network-optimization)
6. [Filesystem Optimization](#filesystem-optimization)
7. [CVar Reference](#cvar-reference)
8. [Benchmarking](#benchmarking)
9. [Troubleshooting](#troubleshooting)

## Performance Profiling

### Built-in Performance Counters

The engine includes built-in performance counters accessible via console command:

```
/perf
```

**Metrics Displayed:**
- FPS (current and average)
- CPU Frame Time (current, average, min, max)
- GPU Frame Time (when available)
- Draw Calls (current, average, min, max, total)

**ImGui Overlay:**
Enable the profiler overlay for real-time monitoring:
```
/set cl_imgui_debug_profiler 1
```

### Tracy Profiler

For detailed profiling, use Tracy Profiler:

**Build with Tracy:**
```bash
cmake .. -DUSE_TRACY=ON
make
```

**Usage:**
1. Run the engine
2. Connect Tracy client to capture profiling data
3. Analyze hotspots and bottlenecks

**Integration Points:**
- `PROF_ZONE_BEGIN()` / `PROF_ZONE_END()` macros throughout codebase
- Automatic zones for major subsystems (Com_Printf, Z_TagMalloc, Event Processing)

### GPU Timing Queries

GPU frame time is automatically measured using Vulkan timestamp queries and integrated into performance counters. This provides accurate GPU-side timing separate from CPU timing.

**View GPU Timing:**
```
/perf
```

Or enable ImGui profiler overlay to see CPU/GPU ratio.

## CPU Optimization

### Frame Time Targets

- **60 FPS**: 16.67ms per frame
- **120 FPS**: 8.33ms per frame
- **144 FPS**: 6.94ms per frame

### Optimization Strategies

#### 1. Reduce Draw Calls

**Problem**: High draw call count increases CPU overhead.

**Solutions:**
- Enable VBO caching for static geometry
- Use instanced rendering where possible
- Batch similar geometry together
- Reduce overdraw with proper culling

**Monitor:**
```
/perf  # Check draw call count
/set r_speeds 1  # Detailed renderer stats
```

#### 2. Optimize Hot Paths

**Identify Hot Paths:**
- Use Tracy Profiler to find functions taking >1ms
- Enable performance counters to track frame time spikes
- Profile with `-DENABLE_COVERAGE=ON` to find frequently executed code

**Common Hot Paths:**
- `Com_Printf()` - Use structured logging with filtering
- `Z_TagMalloc()` / `Z_Free()` - Minimize allocations per frame
- Event processing - Batch events, use deferred processing
- String operations - Use cached paths, avoid repeated allocations

#### 3. Reduce Memory Allocations

**Problem**: Frequent allocations cause frame time spikes.

**Solutions:**
- Pre-allocate buffers at startup
- Use memory pools for frequent allocations
- Reuse temporary buffers
- Minimize string allocations

**Monitor:**
```
/memstats  # Check memory allocation patterns
```

#### 4. Optimize String Operations

**Filesystem Caching:**
```
/set fs_pathNormCache 1  # Cache normalized paths (~30-40% reduction)
/set fs_pathCache 1      # Cache path lookups (~50-60% reduction)
/set fs_existenceCache 1 # Cache file existence checks (~70-80% reduction)
```

**Best Practices:**
- Use `Q_strncpyz()` instead of `strcpy()`
- Cache frequently accessed strings
- Avoid repeated path normalization

## GPU Optimization

### Renderer Settings

#### Vulkan Renderer

**Recommended Settings:**
```
/set r_renderer vulkan
/set r_vk_msaa 4          # 4x MSAA (adjust based on GPU)
/set r_vk_anisotropy 16   # 16x anisotropic filtering
/set r_vk_shadows 1       # Enable shadows
```

#### OpenGL Renderer

**Recommended Settings:**
```
/set r_renderer opengl
/set r_ext_multisample 1
/set r_ext_anisotropic 1
/set r_ext_texture_filter_anisotropic 16
```

### Texture Optimization

**Compression:**
- Use compressed texture formats (DXT/BC, ASTC, ETC2)
- Enable `r_ext_compressed_textures 1`
- Pre-compress textures in asset pipeline

**Mipmaps:**
- Always generate mipmaps for textures
- Use appropriate mipmap filtering
- Set `r_textureMode GL_LINEAR_MIPMAP_LINEAR`

**Size:**
- Use appropriate texture sizes (power of 2)
- Avoid oversized textures
- Use texture atlases where possible

### Shader Optimization

**Best Practices:**
- Minimize texture samples
- Use texture arrays instead of multiple textures
- Optimize shader math (use `mad` instructions)
- Avoid dynamic branching in hot paths
- Use appropriate precision (mediump vs highp)

**Shader Caching:**
- Enable persistent pipeline cache (Vulkan)
- Pre-warm shaders at level load
- Monitor shader compilation time

### Culling and Occlusion

**Enable Culling:**
```
/set r_cull 1              # Frustum culling
/set r_occlusion 1        # Occlusion culling (if available)
```

**Optimize BSP:**
- Ensure proper BSP compilation
- Use `-vis` for visibility optimization
- Use `-light` for lighting optimization

### Draw Call Optimization

**Monitor Draw Calls:**
```
/perf  # Check draw call statistics
/set r_speeds 1  # Detailed renderer information
```

**Reduce Draw Calls:**
- Enable VBO caching: `r_vbo_cache 1`
- Use instanced rendering for repeated geometry
- Batch similar materials together
- Reduce overdraw with proper depth testing

## Memory Optimization

### Memory Tracking

**Enable Memory Tracking:**
```
/set memtrack_enable 1
/set memtrack_report_leaks 1
```

**View Statistics:**
```
/memstats  # Detailed memory statistics by tag
```

**ImGui Overlay:**
```
/set cl_imgui_debug_memory 1
```

### Memory Allocation Strategies

#### Zone Allocator

The zone allocator (`Z_TagMalloc`/`Z_Free`) is optimized for game workloads:

**Tags:**
- `TAG_GENERAL` - General allocations
- `TAG_RENDERER` - Renderer-specific
- `TAG_SOUND` - Audio system
- `TAG_TEMP` - Temporary allocations

**Best Practices:**
- Use appropriate tags for tracking
- Free temporary allocations promptly
- Avoid memory leaks (use `memtrack_enable`)

#### Memory Pools

For frequent allocations, consider memory pools:
- Pre-allocate pools at startup
- Reuse pool allocations
- Monitor pool usage with `memstats`

### Memory Leak Detection

**Development Builds:**
```bash
cmake .. -DENABLE_SANITIZERS=ON
make
```

**Runtime Detection:**
```
/set memtrack_enable 1
/set memtrack_report_leaks 1
/set memtrack_log_leaks 1
```

**Valgrind:**
```bash
./tools/run_valgrind.sh ./idtech3.x86_64
```

## Network Optimization

### Bandwidth Optimization

**Snapshot Rate:**
```
/set sv_fps 20   # Server frame rate (default: 20)
/set cl_maxpackets 125  # Client packet rate
```

**Compression:**
- Enable network compression
- Use delta compression for snapshots
- Minimize snapshot size

### Latency Optimization

**Client Prediction:**
- Enable client-side prediction
- Reduce input lag
- Optimize interpolation

**Server Optimization:**
- Minimize server frame time
- Optimize entity updates
- Use efficient networking code paths

### Monitoring

**Network Stats:**
```
/set cl_imgui_debug_network 1  # Network overlay
/net_stats  # Network statistics (if available)
```

## Filesystem Optimization

### Caching

**Enable All Caches:**
```
/set fs_pathNormCache 1   # Path normalization cache
/set fs_pathCache 1       # Path lookup cache
/set fs_existenceCache 1  # File existence cache
```

**Cache Sizes:**
```
/set fs_cacheSize 1024    # Path cache size (default: 1024)
```

**Impact:**
- Path normalization: ~30-40% reduction
- Path lookup: ~50-60% reduction
- File existence: ~70-80% reduction in stat() calls

### File Access Patterns

**Best Practices:**
- Keep file paths short
- Use relative paths
- Minimize file opens/closes
- Batch file operations
- Use SSD storage when possible

**Handle Caching:**
- File handles are cached automatically (max 384 handles)
- Reduces open/close overhead
- Monitor with filesystem profiling

## CVar Reference

### Performance-Related CVars

#### Renderer
```
r_renderer          - Renderer backend (opengl/vulkan)
r_vk_msaa          - Vulkan MSAA samples (0/2/4/8)
r_vk_anisotropy    - Anisotropic filtering (0-16)
r_vbo_cache        - Enable VBO caching (0/1)
r_cull             - Enable frustum culling (0/1)
r_speeds           - Renderer performance stats (0-6)
```

#### Filesystem
```
fs_pathNormCache   - Path normalization cache (0/1)
fs_pathCache       - Path lookup cache (0/1)
fs_existenceCache  - File existence cache (0/1)
fs_cacheSize       - Path cache size (default: 1024)
```

#### Memory
```
memtrack_enable        - Enable memory tracking (0/1)
memtrack_report_leaks - Report leaks on shutdown (0/1)
memtrack_log_leaks     - Log leaks to file (0/1)
```

#### Performance Counters
```
cl_imgui_debug_performance - Performance overlay (0/1)
cl_imgui_debug_profiler    - Profiler overlay (0/1)
```

#### Network
```
sv_fps           - Server frame rate (default: 20)
cl_maxpackets    - Client packet rate (default: 125)
```

## Benchmarking

### Built-in Benchmarks

**Run Benchmarks:**
```bash
./tools/run_benchmarks.sh
```

**Metrics:**
- Startup time
- Memory usage
- Binary size
- Build time

### Custom Benchmarking

**Frame Time Measurement:**
```
/timedemo <demo>  # Run timedemo
/perf             # View performance counters
```

**Profiling:**
1. Enable Tracy Profiler
2. Capture profiling session
3. Analyze hotspots
4. Compare before/after optimizations

### Performance Regression Testing

**CI Integration:**
- Automated benchmarks in CI
- Performance regression gates
- Threshold-based failure (fail if >X% regression)

**Local Testing:**
```bash
# Run benchmarks
./tools/run_benchmarks.sh

# Compare results
diff benchmark_before.txt benchmark_after.txt
```

## Troubleshooting

### High CPU Usage

**Symptoms:**
- Low FPS despite low GPU usage
- High frame time variance
- CPU bottleneck

**Diagnosis:**
1. Enable performance counters: `/perf`
2. Check draw call count
3. Profile with Tracy
4. Check memory allocation patterns: `/memstats`

**Solutions:**
- Reduce draw calls
- Optimize hot paths
- Reduce memory allocations
- Enable filesystem caches

### High GPU Usage

**Symptoms:**
- High GPU utilization
- Low FPS with high GPU usage
- GPU bottleneck

**Diagnosis:**
1. Check GPU frame time: `/perf`
2. Enable renderer stats: `/set r_speeds 1`
3. Check texture memory usage
4. Monitor draw calls

**Solutions:**
- Reduce texture sizes
- Lower MSAA samples
- Reduce shadow quality
- Optimize shaders
- Enable culling

### Memory Issues

**Symptoms:**
- Memory leaks
- High memory usage
- Out of memory errors

**Diagnosis:**
1. Enable memory tracking: `/set memtrack_enable 1`
2. Check memory stats: `/memstats`
3. Run with sanitizers: `-DENABLE_SANITIZERS=ON`
4. Use Valgrind: `./tools/run_valgrind.sh`

**Solutions:**
- Fix memory leaks
- Reduce memory allocations
- Use memory pools
- Optimize data structures

### Network Issues

**Symptoms:**
- High latency
- Packet loss
- Network lag

**Diagnosis:**
1. Enable network overlay: `/set cl_imgui_debug_network 1`
2. Check network stats
3. Monitor packet rates

**Solutions:**
- Adjust snapshot rate
- Optimize network code
- Reduce snapshot size
- Enable compression

## Best Practices

### Development

1. **Profile First**: Always profile before optimizing
2. **Measure Changes**: Benchmark before/after optimizations
3. **Use Tools**: Leverage Tracy, sanitizers, and built-in counters
4. **Monitor Metrics**: Track FPS, frame time, memory, draw calls
5. **Test Thoroughly**: Ensure optimizations don't break functionality

### Code Optimization

1. **Hot Paths**: Optimize frequently executed code first
2. **Memory**: Minimize allocations, use pools, fix leaks
3. **Caching**: Enable filesystem caches, cache computations
4. **Batching**: Batch operations, reduce draw calls
5. **Algorithms**: Use efficient algorithms and data structures

### User Configuration

1. **Hardware**: Use appropriate settings for hardware
2. **Resolution**: Match resolution to GPU capability
3. **Quality**: Balance quality vs performance
4. **Monitoring**: Use performance overlays to identify bottlenecks
5. **Updates**: Keep drivers and engine updated

## Related Documentation

- [Optimization and Stability](optimization-and-stability.md) - Existing optimizations
- [Memory Safety Workflows](MEMORY_SAFETY_WORKFLOWS.md) - Memory debugging
- [Code Coverage](CODE_COVERAGE.md) - Finding hot paths
- [Static Analysis Workflow](STATIC_ANALYSIS_WORKFLOW.md) - Code quality

## Summary

**Key Performance Metrics:**
- **FPS**: Target 60+ FPS (16.67ms frame time)
- **Draw Calls**: Minimize per frame (<1000 ideal)
- **Memory**: Track leaks, optimize allocations
- **GPU Time**: Monitor GPU frame time vs CPU frame time

**Essential Tools:**
- Built-in performance counters (`/perf`)
- Tracy Profiler (detailed profiling)
- Memory tracking (`/memstats`)
- ImGui debug overlays

**Quick Wins:**
1. Enable filesystem caches
2. Enable VBO caching
3. Reduce draw calls
4. Fix memory leaks
5. Optimize hot paths

Following this guide will help achieve optimal performance while maintaining code quality and stability.
