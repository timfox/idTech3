# Geometry Clipmaps: Terrain Rendering Using Nested Regular Grids

## Overview

Geometry clipmaps provide an efficient method for rendering large terrain height fields using nested regular grids centered about the viewer. This technique caches terrain data in video memory as vertex buffers and incrementally updates them as the viewpoint moves, providing visual continuity, uniform frame rates, and support for compression and synthesis.

## Key Concepts

### Clipmap Structure

A geometry clipmap consists of `m` levels, each containing an `n×n` array of vertices stored as a vertex buffer in video memory. Each level represents the terrain at a different resolution (power-of-two spacing). Levels are accessed toroidally (with 2D wraparound) for efficient incremental updates.

### Regions

For each clipmap level `l`, three regions are defined:

- **Clip Region**: The world extent of the `n×n` grid of data stored at that level
- **Active Region**: The square of size `n×g_l × n×g_l` centered at the viewer (where `g_l = 2^-l` is the grid spacing)
- **Render Region**: The hollowed frame whose outer perimeter is `active_region(l)` and inner perimeter is `active_region(l+1)`

### Transition Morphing

To eliminate gaps and provide temporal continuity, geometry near the outer boundary of each render region is morphed to transition smoothly to the coarser level. The morph uses a blend parameter `α` computed from spatial grid coordinates:

```
z' = (1 - α) * z + α * z_c
```

where `z` is the fine level height and `z_c` is the coarse level height. The transition width `w` is typically `n/10` grid units.

## Shaders

### `geometry_clipmap_vert.glsl`

Vertex shader that:
- Reconstructs world positions from grid coordinates
- Samples height data using toroidal addressing
- Computes transition blend factor `α` for geometry morphing
- Blends fine and coarse heights for smooth transitions

**Key Features:**
- Toroidal texture addressing for efficient updates
- Spatial transition region computation (~10 GPU instructions)
- Support for multiple clipmap levels

### `geometry_clipmap_frag.glsl`

Fragment shader that:
- Blends normal maps from fine and coarse levels using transition `α`
- Blends color textures using the same transition parameter
- Performs simple lighting calculations

**Key Features:**
- Unified LOD for geometry and textures
- Smooth texture transitions without mipmap artifacts
- Efficient texture sampling with configurable tiling

### `terrain_synthesis.comp`

Compute shader for generating fine-level terrain detail:
- Uses four-point interpolatory subdivision to predict fine level from coarse
- Applies fractal noise displacement for detail
- Supports multiple octaves for realistic terrain variation

**Parameters:**
- `noiseScale`: Scale factor for noise sampling
- `noiseAmplitude`: Amplitude of displacement
- `noiseOctaves`: Number of octaves (typically 3-5)
- `variance`: Target variance matching actual terrain statistics

### `terrain_normal_gen.comp`

Compute shader for generating normal maps:
- Computes normals using central differences on height data
- Stores normals in texture format (RGBA8, using RGB channels)
- Normal maps are generated at 2x resolution of geometry for faithful shading

## Usage

### Basic Setup

1. **Initialize Clipmap Levels**: Create `m` levels, each with an `n×n` vertex buffer
2. **Load/Generate Height Data**: Load compressed terrain or generate procedurally
3. **Update Active Regions**: Each frame, compute desired active regions based on viewer position
4. **Update Clip Regions**: Incrementally update clip regions as viewer moves
5. **Render**: Render all levels in fine-to-coarse order

### Update Algorithm

```glsl
// Per-frame update
foreach level l in coarse-to-fine order:
    // Compute desired active region (centered at viewer)
    desired_active = square(n * g_l) centered at viewer
    
    // Update clip region if needed
    if (clip_region needs update):
        fill newly exposed L-shaped region
        // Data from decompression or synthesis
    
    // Crop active region to available data
    active_region = desired_active ∩ clip_region
    active_region = active_region ∩ (active_region(l-1) ⊖ 2)

// Render
foreach level l in fine-to-coarse order:
    render_region = active_region(l) - active_region(l+1)
    Render render_region with transition morphing
```

### Compression

Terrain data can be stored in compressed form:
1. Create terrain pyramid by downsampling
2. Predict each level from coarser level using subdivision
3. Compress residuals between predicted and actual data
4. Decompress only the clipmap regions as needed

Compression factors of 60-100 are typical for height map data.

### Synthesis

For levels finer than stored data:
1. Predict fine level from coarse using subdivision
2. Add fractal noise displacement scaled to match terrain variance
3. Store precomputed noise in texture for deterministic synthesis

## Performance Characteristics

### Rendering

- **Throughput**: ~60 M∆/sec (millions of triangles per second)
- **Frame Rate**: 60+ fps for typical clipmap sizes (n=255, m=11)
- **Memory**: ~11 MB video memory for geometry (16 bytes per vertex × m levels × n²)
- **Normal Maps**: ~6 MB additional (2 bytes per sample, 2x resolution)

### Updates

Update times for a full `n×n` level (n=255):
- Computation of `z_c`: 2 ms
- Interpolatory subdivision: 3 ms
- Decompression or Synthesis: 8 ms or 3 ms
- Upload to video memory: 2 ms
- Normal map computation: 11 ms
- **Total**: 21-26 ms

Updates are incremental and typically only affect small regions, so per-frame costs are much lower.

## Configuration

### Clipmap Size

Default clipmap size `n=255` provides:
- Screen-space triangle size of ~3 pixels (at 640×480, 90° FOV)
- Normal map resolution of ~1.5 pixels per sample
- Good balance between quality and performance

### Transition Width

Transition width `w = n/10` provides:
- Smooth visual continuity
- Minimal loss of fine detail
- Efficient GPU computation

### Level Count

Number of levels `m` depends on terrain extent:
- For 16K×16K terrain: m ≈ 9-10 levels
- For 216K×94K terrain: m ≈ 11 levels
- Each level doubles the world-space extent

## Limitations

1. **Mesh Complexity**: Rendered mesh is more complex than adaptive LOD schemes, assuming worst-case uniform detail
2. **Spectral Density**: Terrain must have bounded spectral density (no needle-like features)
3. **Memory**: All clipmap data must fit in video memory (typically 10-20 MB)

## Advantages

1. **Simplicity**: No irregular data structures or refinement dependencies
2. **Optimal Throughput**: Regular grids enable optimal vertex cache reuse
3. **Visual Continuity**: Smooth transitions in both space and time
4. **Steady Rendering**: Constant frame rate independent of terrain roughness
5. **Compression**: Enables 60-100x compression of terrain data
6. **Synthesis**: Supports procedural detail generation
7. **Unified LOD**: Same structure for geometry and textures
