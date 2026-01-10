// Path Tracing of Signed Distance Function Grids

#version 450

// SDF Grid Ray Tracing Implementation

// ============================================================================
// Section 2: Analytic Voxel Intersection
// ============================================================================

// Constants for a single voxel (2x2x2 signed distance values)
// sijk with i, j, k ∈ {0, 1}
struct VoxelSDF {
    float s000, s100, s010, s110;  // z=0 plane
    float s001, s101, s011, s111;  // z=1 plane
};

// Compute constants ki from SDF values (Equation 3)
void computeConstants(in VoxelSDF sdf, out float k[8]) {
    k[0] = sdf.s000;
    k[1] = sdf.s100 - sdf.s000;
    k[2] = sdf.s010 - sdf.s000;
    k[3] = sdf.s110 - sdf.s010 - k[1];
    
    float a = sdf.s101 - sdf.s001;
    k[4] = k[0] - sdf.s001;
    k[5] = k[1] - a;
    k[6] = k[2] - (sdf.s011 - sdf.s001);
    k[7] = k[3] - (sdf.s111 - sdf.s011 - a);
}

// Compute cubic polynomial coefficients (Equations 6-7)
// Ray: r(t) = o + t*d, where o is origin and d is direction
void computeCubicCoeffs(
    in float k[8],
    in vec3 o,  // ray origin in voxel space [0,1]^3
    in vec3 d,  // ray direction
    out float c[4]  // c3*t^3 + c2*t^2 + c1*t + c0 = 0
) {
    // Precompute shared values (m0-m5 from Equation 7)
    float m0 = o.x * o.y;
    float m1 = d.x * d.y;
    float m2 = o.x * d.y + o.y * d.x;
    float m3 = k[5] * o.z - k[1];
    float m4 = k[6] * o.z - k[2];
    float m5 = k[7] * o.z - k[3];
    
    // Compute cubic coefficients (Equation 6)
    c[0] = (k[4] * o.z - k[0]) + o.x * m3 + o.y * m4 + m0 * m5;
    c[1] = d.x * m3 + d.y * m4 + m2 * m5 + d.z * (k[4] + k[5] * o.x + k[6] * o.y + k[7] * m0);
    c[2] = m1 * m5 + d.z * (k[5] * d.x + k[6] * d.y + k[7] * m2);
    c[3] = k[7] * m1 * d.z;
}

// Evaluate cubic polynomial: g(t) = c3*t^3 + c2*t^2 + c1*t + c0
float evalCubic(in float c[4], float t) {
    return c[0] + t * (c[1] + t * (c[2] + t * c[3]));
}

// Evaluate quadratic (derivative): g'(t) = 3*c3*t^2 + 2*c2*t + c1
float evalQuadratic(in float c[4], float t) {
    return c[1] + t * (2.0 * c[2] + 3.0 * t * c[3]);
}

// Analytic cubic root solver using Vieta's approach
// Returns the number of real roots found, and stores roots in roots[] array
// Only returns roots in [0, tfar] range
int solveCubicAnalytic(in float c[4], float tfar, out float roots[3]) {
    // Handle degenerate cases
    if (abs(c[3]) < 1e-10) {
        // Quadratic case: c2*t^2 + c1*t + c0 = 0
        if (abs(c[2]) < 1e-10) {
            // Linear case: c1*t + c0 = 0
            if (abs(c[1]) < 1e-10) {
                return 0; // No solution
            }
            float t = -c[0] / c[1];
            if (t >= 0.0 && t <= tfar) {
                roots[0] = t;
                return 1;
            }
            return 0;
        }
        
        // Quadratic formula
        float disc = c[1] * c[1] - 4.0 * c[2] * c[0];
        if (disc < 0.0) return 0;
        
        float sqrtDisc = sqrt(disc);
        float t1 = (-c[1] - sqrtDisc) / (2.0 * c[2]);
        float t2 = (-c[1] + sqrtDisc) / (2.0 * c[2]);
        
        int count = 0;
        if (t1 >= 0.0 && t1 <= tfar) roots[count++] = t1;
        if (t2 >= 0.0 && t2 <= tfar && abs(t2 - t1) > 1e-6) roots[count++] = t2;
        return count;
    }
    
    // Normalize to monic form: t^3 + a*t^2 + b*t + c = 0
    float a = c[2] / c[3];
    float b = c[1] / c[3];
    float c_norm = c[0] / c[3];
    
    // Depressed cubic: u^3 + p*u + q = 0, where u = t + a/3
    float p = b - a * a / 3.0;
    float q = c_norm + (2.0 * a * a * a - 9.0 * a * b) / 27.0;
    
    float disc = q * q / 4.0 + p * p * p / 27.0;
    
    int count = 0;
    if (disc > 0.0) {
        // One real root
        float sqrtDisc = sqrt(disc);
        float u = cbrt(-q / 2.0 + sqrtDisc) + cbrt(-q / 2.0 - sqrtDisc);
        float t = u - a / 3.0;
        if (t >= 0.0 && t <= tfar) {
            roots[count++] = t;
        }
    } else if (disc < 0.0) {
        // Three real roots
        float angle = acos(-q / 2.0 * sqrt(-27.0 / (p * p * p))) / 3.0;
        float r = 2.0 * sqrt(-p / 3.0);
        
        for (int i = 0; i < 3; i++) {
            float u = r * cos(angle - 2.0 * 3.14159265359 * float(i) / 3.0);
            float t = u - a / 3.0;
            if (t >= 0.0 && t <= tfar) {
                roots[count++] = t;
            }
        }
    } else {
        // Discriminant == 0: multiple roots
        float u = 3.0 * q / p;
        float t1 = u - a / 3.0;
        float t2 = -u / 2.0 - a / 3.0;
        
        if (t1 >= 0.0 && t1 <= tfar) roots[count++] = t1;
        if (t2 >= 0.0 && t2 <= tfar && abs(t2 - t1) > 1e-6) roots[count++] = t2;
    }
    
    return count;
}

// Newton-Raphson refinement (Listing 1 from paper)
float refineRootNewtonRaphson(
    in float c[4],
    float tInitial,
    float tfar,
    int maxIterations,
    float epsilon
) {
    float t = clamp(tInitial, 0.0, tfar);
    float tPrev = t + 2.0 * epsilon; // Ensure first iteration runs
    
    for (int i = 0; i < maxIterations && abs(t - tPrev) >= epsilon; i++) {
        float gt = evalCubic(c, t);
        float gtDeriv = evalQuadratic(c, t);
        
        if (abs(gtDeriv) < 1e-10) break; // Avoid division by zero
        
        tPrev = t;
        t -= gt / gtDeriv;
        t = clamp(t, 0.0, tfar);
    }
    
    return t;
}

// Marmitt et al. method: split polynomial using derivative roots
// Returns true if a root exists in [0, tfar], and stores refined t
bool findRootMarmitt(
    in float c[4],
    float tfar,
    out float t
) {
    // Solve g'(t) = 0: 3*c3*t^2 + 2*c2*t + c1 = 0
    float quadC[3];
    quadC[0] = c[1];
    quadC[1] = 2.0 * c[2];
    quadC[2] = 3.0 * c[3];
    
    float derivRoots[2];
    int derivCount = 0;
    
    if (abs(quadC[2]) < 1e-10) {
        // Linear derivative
        if (abs(quadC[1]) > 1e-10) {
            float root = -quadC[0] / quadC[1];
            if (root >= 0.0 && root <= tfar) {
                derivRoots[derivCount++] = root;
            }
        }
    } else {
        // Quadratic derivative
        float disc = quadC[1] * quadC[1] - 4.0 * quadC[2] * quadC[0];
        if (disc >= 0.0) {
            float sqrtDisc = sqrt(disc);
            float r1 = (-quadC[1] - sqrtDisc) / (2.0 * quadC[2]);
            float r2 = (-quadC[1] + sqrtDisc) / (2.0 * quadC[2]);
            if (r1 >= 0.0 && r1 <= tfar) derivRoots[derivCount++] = r1;
            if (r2 >= 0.0 && r2 <= tfar && abs(r2 - r1) > 1e-6) derivRoots[derivCount++] = r2;
        }
    }
    
    // Sort derivative roots
    if (derivCount == 2 && derivRoots[0] > derivRoots[1]) {
        float tmp = derivRoots[0];
        derivRoots[0] = derivRoots[1];
        derivRoots[1] = tmp;
    }
    
    // Check intervals for sign changes
    float tStart = 0.0;
    for (int i = 0; i <= derivCount; i++) {
        float tEnd = (i < derivCount) ? derivRoots[i] : tfar;
        
        float gStart = evalCubic(c, tStart);
        float gEnd = evalCubic(c, tEnd);
        
        // Sign change indicates a root
        if (gStart * gEnd <= 0.0) {
            // Refine using Newton-Raphson
            float tInitial = (gEnd - gStart != 0.0) ? 
                (gEnd * tStart - gStart * tEnd) / (gEnd - gStart) : 
                (tStart + tEnd) * 0.5;
            t = refineRootNewtonRaphson(c, tInitial, tfar, 50, 4e-3);
            return true;
        }
        
        tStart = tEnd;
    }
    
    return false;
}

// Intersect ray with SDF surface in a voxel
// Returns true if intersection found, stores t and hit point
bool intersectVoxelSDF(
    in VoxelSDF sdf,
    in vec3 rayOrigin,      // in voxel space [0,1]^3
    in vec3 rayDir,         // normalized direction
    in float tfar,          // maximum t (exit point of voxel)
    out float t,            // intersection distance
    out vec3 hitPoint       // intersection point
) {
    // Compute constants
    float k[8];
    computeConstants(sdf, k);
    
    // Compute cubic coefficients
    float c[4];
    computeCubicCoeffs(k, rayOrigin, rayDir, c);
    
    // Try analytic solver first
    float roots[3];
    int rootCount = solveCubicAnalytic(c, tfar, roots);
    
    if (rootCount > 0) {
        // Find first valid root
        t = roots[0];
        for (int i = 1; i < rootCount; i++) {
            if (roots[i] < t && roots[i] >= 0.0) {
                t = roots[i];
            }
        }
        hitPoint = rayOrigin + rayDir * t;
        return true;
    }
    
    // Fallback to Marmitt method
    if (findRootMarmitt(c, tfar, t)) {
        hitPoint = rayOrigin + rayDir * t;
        return true;
    }
    
    return false;
}

// ============================================================================
// Section 3: Normals
// ============================================================================

// Section 3.1: Analytic Normal Computation
// Compute normal using analytic derivative of trilinear interpolation
vec3 computeAnalyticNormal(in VoxelSDF sdf, in vec3 p) {
    // p is in voxel space [0,1]^3
    
    // Equation 9: ∂f/∂x
    float y0 = mix(sdf.s100 - sdf.s000, sdf.s110 - sdf.s010, p.y);
    float y1 = mix(sdf.s101 - sdf.s001, sdf.s111 - sdf.s011, p.y);
    float dfdx = mix(y0, y1, p.z);
    
    // Equation 10: ∂f/∂y
    float x0 = mix(sdf.s010 - sdf.s000, sdf.s110 - sdf.s100, p.x);
    float x1 = mix(sdf.s011 - sdf.s001, sdf.s111 - sdf.s101, p.x);
    float dfdy = mix(x0, x1, p.z);
    
    // Equation 11: ∂f/∂z
    x0 = mix(sdf.s001 - sdf.s000, sdf.s101 - sdf.s100, p.x);
    x1 = mix(sdf.s011 - sdf.s010, sdf.s111 - sdf.s110, p.x);
    float dfdz = mix(x0, x1, p.y);
    
    vec3 n = vec3(dfdx, dfdy, dfdz);
    return normalize(n);
}

// Section 3.2: Continuous Normals Across Voxels
// Interpolate normals from 2x2x2 neighboring voxels using dual voxel concept
vec3 computeContinuousNormal(
    in VoxelSDF voxels[8],  // 2x2x2 voxels around hit point
    in vec3 hitPoint,       // in dual voxel space [0,1]^3
    in ivec3 voxelIndices   // which voxel in the 2x2x2 grid contains the hit
) {
    // Compute normals in each of the 8 voxels at the hit point
    vec3 normals[8];
    for (int i = 0; i < 8; i++) {
        // Convert hit point to each voxel's local space
        ivec3 idx = ivec3(
            (i & 1) != 0 ? 1 : 0,
            (i & 2) != 0 ? 1 : 0,
            (i & 4) != 0 ? 1 : 0
        );
        
        // Hit point in this voxel's space (may be outside [0,1]^3)
        vec3 localP = hitPoint - vec3(idx);
        normals[i] = computeAnalyticNormal(voxels[i], localP);
    }
    
    // Trilinear interpolation of normals (Equation 12)
    vec3 uvw = hitPoint; // Position in dual voxel [0,1]^3
    vec3 n = 
        (1.0 - uvw.x) * (1.0 - uvw.y) * (1.0 - uvw.z) * normals[0] +
        uvw.x * (1.0 - uvw.y) * (1.0 - uvw.z) * normals[1] +
        (1.0 - uvw.x) * uvw.y * (1.0 - uvw.z) * normals[2] +
        uvw.x * uvw.y * (1.0 - uvw.z) * normals[3] +
        (1.0 - uvw.x) * (1.0 - uvw.y) * uvw.z * normals[4] +
        uvw.x * (1.0 - uvw.y) * uvw.z * normals[5] +
        (1.0 - uvw.x) * uvw.y * uvw.z * normals[6] +
        uvw.x * uvw.y * uvw.z * normals[7];
    
    return normalize(n);
}

// ============================================================================
// Utility Functions
// ============================================================================

// Trilinear interpolation of SDF value at point p in voxel space [0,1]^3
float trilinearInterpolate(in VoxelSDF sdf, in vec3 p) {
    float x = p.x, y = p.y, z = p.z;
    return 
        (1.0 - z) * (
            (1.0 - y) * ((1.0 - x) * sdf.s000 + x * sdf.s100) +
            y * ((1.0 - x) * sdf.s010 + x * sdf.s110)
        ) +
        z * (
            (1.0 - y) * ((1.0 - x) * sdf.s001 + x * sdf.s101) +
            y * ((1.0 - x) * sdf.s011 + x * sdf.s111)
        );
}

// Ray-box intersection (AABB)
// Returns true if ray intersects box, stores tNear and tFar
bool intersectAABB(
    in vec3 rayOrigin,
    in vec3 rayDir,
    in vec3 boxMin,
    in vec3 boxMax,
    out float tNear,
    out float tFar
) {
    vec3 invDir = 1.0 / rayDir;
    vec3 t0 = (boxMin - rayOrigin) * invDir;
    vec3 t1 = (boxMax - rayOrigin) * invDir;
    
    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);
    
    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar = min(min(tMax.x, tMax.y), tMax.z);
    
    return tFar >= tNear && tFar >= 0.0;
}
