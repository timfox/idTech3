# Material System Capacity Limits

## Overview

The Vulkan renderer includes a runtime material parameter system that allows dynamic modification of material properties (wetness, damage, magic effects, etc.). This system has a fixed capacity that determines how many unique materials can be active simultaneously.

## Current Configuration

- **Capacity**: 2048 materials
- **Memory per material**: 168 bytes (CPU + GPU)
- **Total memory usage**: 336KB CPU + 336KB GPU

## Capacity Constraints

### 1. MAX_DRAWIMAGES Limit (2048)

The material system capacity is tied to `MAX_DRAWIMAGES`, which limits the total number of textures/images the renderer can handle.

**Files to modify for increase:**
- `src/renderer/tr_local.h`
- `src/renderervk/tr_local.h`

**Impact of increasing:**
- Scales Vulkan descriptor pool allocation automatically
- Linear memory scaling: `new_memory = old_memory × (new_capacity ÷ 2048)`
- May require testing on lower-end hardware

### 2. Vulkan Storage Buffer Limits

The material parameters are stored in a GPU storage buffer.

**Hardware-specific limits:**
- `maxStorageBufferRange`: Typically 128MB - 2GB
- Current usage (344KB) is well within all hardware limits

**Theoretical maximum capacity:**
- 128MB limit: ~786,432 materials
- 2GB limit: ~12.5 million materials
- Practical limit lower due to descriptor constraints

### 3. Descriptor Pool Limits

The Vulkan descriptor pool allocates descriptors based on `MAX_DRAWIMAGES`:

- Combined image samplers: `MAX_DRAWIMAGES + overhead`
- Storage buffers: 1 (material parameter buffer)
- Pool scales automatically with `MAX_DRAWIMAGES`

### 4. Memory Constraints

- **CPU memory**: Zone-allocated material parameter storage
- **GPU memory**: Storage buffer for shader access
- **No additional per-material GPU resources**

## Increasing Capacity

### Step-by-Step Process

1. **Update MAX_DRAWIMAGES** in both header files:
   ```c
   #define MAX_DRAWIMAGES 4096  // or desired value
   ```

2. **Rebuild engine** - descriptor pools scale automatically

3. **Test on target hardware** for:
   - Storage buffer size limits
   - Descriptor pool availability
   - Memory pressure

### Memory Scaling Calculator

For new capacity `N`:

- CPU memory: `336KB × (N ÷ 2048)`
- GPU memory: `336KB × (N ÷ 2048)`
- Descriptor overhead: Scales with `MAX_DRAWIMAGES`

### Recommended Capacities

- **2048** (current): Sufficient for most games
- **4096**: Good for large open-world games with many material variants
- **8192**: High-end applications with extensive material variation
- **16384+**: Specialized applications (may hit hardware limits)

## Hardware Compatibility

### Minimum Requirements
- Vulkan 1.1+ capable GPU
- Storage buffer support (ubiquitous on modern hardware)

### Memory Requirements
- Additional ~688KB per 2048 materials
- Linear scaling with capacity

### Performance Impact
- Minimal CPU overhead (zone allocation only during init)
- GPU overhead: One storage buffer bind per frame
- Shader access: Fast constant-time lookups

## Troubleshooting

### Descriptor Pool Exhaustion
If you hit descriptor limits, reduce `MAX_DRAWIMAGES` or optimize descriptor usage.

### Storage Buffer Limits
If initialization fails on specific hardware, the storage buffer may exceed `maxStorageBufferRange`. Reduce capacity or implement fallback.

### Memory Pressure
Monitor total GPU memory usage. Material system uses minimal memory compared to textures/geometry.

## Future Considerations

The material system could be extended with:
- Dynamic capacity growth (reallocation)
- Compressed parameter storage
- Material streaming/paging
- CPU-side caching to reduce GPU buffer size