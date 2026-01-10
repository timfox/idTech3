// Geometry Clipmap Vertex Shader
// Implements nested regular grids with transition morphing for visual continuity

#version 450

layout(location = 0) in vec3 a_position;      // Grid position (x, y, gridIndex)
layout(location = 1) in vec2 a_texCoord;      // Texture coordinates

layout(location = 0) out vec3 v_worldPos;
layout(location = 1) out vec2 v_texCoord;
layout(location = 2) out float v_transitionAlpha;  // For geometry morphing
layout(location = 3) out float v_level;              // Clipmap level

layout(binding = 0) uniform ClipmapUniforms {
    mat4 viewProj;
    vec3 viewerPos;              // Viewer position in world space
    float gridSpacing;           // Grid spacing for this level (g_l = 2^-l)
    int clipmapSize;             // Size of clipmap grid (n)
    int level;                   // Current clipmap level (l)
    vec2 clipRegionMin;          // Minimum bounds of clip region
    vec2 clipRegionMax;          // Maximum bounds of clip region
    vec2 activeRegionMin;        // Minimum bounds of active region
    vec2 activeRegionMax;        // Maximum bounds of active region
    float transitionWidth;       // Transition region width (w = n/10)
} clipmap;

// Height data stored in texture
layout(binding = 1) uniform sampler2D heightTexture;
layout(binding = 2) uniform sampler2D heightTextureCoarse;  // Next coarser level

// Toroidal addressing: wrap coordinates within clipmap bounds
vec2 toroidalAddress(vec2 coord, vec2 minBound, vec2 maxBound) {
    vec2 size = maxBound - minBound;
    vec2 offset = coord - minBound;
    return minBound + mod(offset, size);
}

// Compute transition blend factor alpha
// Alpha ramps from 0 (inside) to 1 (at outer perimeter)
// Formula: α = max(α_x, α_y) where
// α_x = clamp((|x - v_x| - (x_max - x_min)/2 - w - 1) / w, 0, 1)
float computeTransitionAlpha(vec2 gridPos, vec2 viewerGridPos, 
                             vec2 activeMin, vec2 activeMax, float width) {
    // Compute alpha for X direction
    float halfSizeX = (activeMax.x - activeMin.x) * 0.5;
    float distFromCenterX = abs(gridPos.x - viewerGridPos.x);
    float alphaX = (distFromCenterX - halfSizeX + width + 1.0) / width;
    alphaX = clamp(alphaX, 0.0, 1.0);
    
    // Compute alpha for Y direction
    float halfSizeY = (activeMax.y - activeMin.y) * 0.5;
    float distFromCenterY = abs(gridPos.y - viewerGridPos.y);
    float alphaY = (distFromCenterY - halfSizeY + width + 1.0) / width;
    alphaY = clamp(alphaY, 0.0, 1.0);
    
    // Final alpha is maximum of both directions
    return max(alphaX, alphaY);
}

void main() {
    // Reconstruct grid coordinates from position
    // Position.x = grid X, Position.y = grid Y, Position.z = unused
    ivec2 gridCoord = ivec2(a_position.x, a_position.y);
    vec2 gridPos = vec2(gridCoord);
    
    // Convert to world space coordinates
    vec2 worldPos2D = clipmap.clipRegionMin + gridPos * clipmap.gridSpacing;
    
    // Sample height from height texture using toroidal addressing
    vec2 texCoord = (gridPos - clipmap.clipRegionMin) / 
                    (clipmap.clipRegionMax - clipmap.clipRegionMin);
    texCoord = toroidalAddress(texCoord, vec2(0.0), vec2(1.0));
    
    float height = texture(heightTexture, texCoord).r;
    
    // Sample coarse height for transition morphing
    float heightCoarse = 0.0;
    if (clipmap.level > 0) {
        // Use same texture coordinates for coarse level
        heightCoarse = texture(heightTextureCoarse, texCoord).r;
    }
    
    // Compute transition blend factor
    // Convert viewer world position to grid coordinates
    vec2 viewerWorldPos = vec2(clipmap.viewerPos.x, clipmap.viewerPos.z);
    vec2 viewerGridPos = (viewerWorldPos - clipmap.clipRegionMin) / clipmap.gridSpacing;
    float alpha = computeTransitionAlpha(
        gridPos, 
        viewerGridPos,
        clipmap.activeRegionMin,
        clipmap.activeRegionMax,
        clipmap.transitionWidth
    );
    
    // Morph height: z' = (1 - alpha) * z + alpha * z_c
    float morphedHeight = mix(height, heightCoarse, alpha);
    
    // Final world position
    v_worldPos = vec3(worldPos2D.x, morphedHeight, worldPos2D.y);
    v_texCoord = a_texCoord;
    v_transitionAlpha = alpha;
    v_level = float(clipmap.level);
    
    // Transform to clip space
    gl_Position = clipmap.viewProj * vec4(v_worldPos, 1.0);
}
