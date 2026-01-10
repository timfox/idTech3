# SDF Grid Ray Tracing Implementation

This document describes the implementation of path tracing techniques for Signed Distance Function (SDF) grids:


## Overview

The implementation provides GPU shader code for analytically ray tracing 3D SDF grids. Unlike traditional sphere tracing, this method uses analytic intersection tests against the cubic polynomial surface defined by trilinear interpolation of SDF values within each voxel.

## Key Features

### 1. Analytic Voxel Intersection (Section 2)

The implementation provides functions to:
- Compute cubic polynomial coefficients from SDF corner values
- Solve the cubic equation analytically using Vieta's approach
- Use Newton-Raphson refinement for numerical root finding
- Apply the Marmitt et al. method for polynomial splitting

**Main Function:**
```glsl
bool intersectVoxelSDF(
    in VoxelSDF sdf,
    in vec3 rayOrigin,      // in voxel space [0,1]^3
    in vec3 rayDir,         // normalized direction
    in float tfar,          // maximum t (exit point of voxel)
    out float t,            // intersection distance
    out vec3 hitPoint       // intersection point
)
```

### 2. Normal Computation (Section 3)

Two methods are provided:

#### Analytic Normals (Section 3.1)
Computes normals using the analytic derivative of the trilinear interpolation function. This is fast but produces discontinuous normals at voxel boundaries.

```glsl
vec3 computeAnalyticNormal(in VoxelSDF sdf, in vec3 p)
```

#### Continuous Normals (Section 3.2)
Interpolates normals from 2×2×2 neighboring voxels using a dual voxel concept. This produces smooth, continuous normals across voxel boundaries at the cost of additional computation.

```glsl
vec3 computeContinuousNormal(
    in VoxelSDF voxels[8],  // 2x2x2 voxels around hit point
    in vec3 hitPoint,       // in dual voxel space [0,1]^3
    in ivec3 voxelIndices   // which voxel in the 2x2x2 grid contains the hit
)
```

## Data Structures

### VoxelSDF
Represents the 2×2×2 signed distance values at the corners of a voxel:

```glsl
struct VoxelSDF {
    float s000, s100, s010, s110;  // z=0 plane
    float s001, s101, s011, s111;  // z=1 plane
};
```

## Usage Example

```glsl
#version 450
#include "sdf_grid_raytrace.glsl"

// Example: Intersect a ray with an SDF voxel
void traceSDFVoxel() {
    // Define SDF values at voxel corners
    VoxelSDF sdf;
    sdf.s000 = 0.5; sdf.s100 = -0.3; sdf.s010 = 0.2; sdf.s110 = -0.1;
    sdf.s001 = 0.4; sdf.s101 = -0.2; sdf.s011 = 0.1; sdf.s111 = -0.05;
    
    // Ray in voxel space [0,1]^3
    vec3 rayOrigin = vec3(0.1, 0.1, 0.1);
    vec3 rayDir = normalize(vec3(1.0, 0.5, 0.3));
    float tfar = 2.0; // Maximum distance to voxel exit
    
    // Intersect
    float t;
    vec3 hitPoint;
    if (intersectVoxelSDF(sdf, rayOrigin, rayDir, tfar, t, hitPoint)) {
        // Compute normal at hit point
        vec3 normal = computeAnalyticNormal(sdf, hitPoint);
        
        // Use normal for shading...
    }
}
```

## Performance Characteristics

According to the paper, the fastest methods are:

1. **SVS-A** (Sparse Voxel Set with Analytic intersection): ~9-17ms per frame
2. **SVS-M** (Sparse Voxel Set with Marmitt method): ~10-17ms per frame

The analytic method (A) is generally fastest, with the Marmitt method (M) providing similar performance in many cases.

### Normal Computation Performance

- **Analytic normals**: Similar performance to baseline (0-0.3% overhead)
- **Continuous normals**: 1-7% overhead, but provides significantly better visual quality for close-up views

## Integration with idTech3

To integrate this into the Vulkan renderer:

1. **Include the shader**: Add `#include "sdf_grid_raytrace.glsl"` to your shader
2. **Load SDF grid data**: Store SDF grids in 3D textures or structured buffers
3. **Traversal**: Implement one of the traversal methods:
   - Grid Sphere Tracing (GST)
   - Sparse Voxel Set (SVS) with BVH
   - Sparse Brick Set (SBS)
   - Sparse Voxel Octree (SVO)
4. **Intersection**: Use `intersectVoxelSDF()` when a voxel is reached
5. **Normals**: Choose between analytic or continuous based on quality/performance needs

## Shadow Ray Optimization

The paper describes an optimization for shadow rays: when using polynomial splitting methods (Marmitt or Newton-Raphson), you can terminate early if a root interval fully overlaps with `[0, tlight]` without needing numeric iteration.

This optimization provides:
- 4-6% speedup for GST-NR
- 6-14% speedup for SVS-NR
- 1-2% speedup for SBS-NR
- 0.6-1.3% speedup for SVO-NR
