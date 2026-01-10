// Simplified SDF Vector Texture Shader
// Works with alpha testing (threshold 0.5) or soft edges

#version 450

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// SDF texture (distance field stored in alpha channel)
layout(binding = 0) uniform sampler2D baseTextureSampler;

// Optional base color texture
layout(binding = 1) uniform sampler2D baseColorTexture;

// Uniforms matching the paper's implementation
layout(binding = 2) uniform SDFUniforms {
    // Soft edges
    bool SOFT_EDGES;
    float SOFT_EDGE_MIN;
    float SOFT_EDGE_MAX;
    
    // Outlining
    bool OUTLINE;
    float OUTLINE_MIN_VALUE0;
    float OUTLINE_MIN_VALUE1;
    float OUTLINE_MAX_VALUE0;
    float OUTLINE_MAX_VALUE1;
    vec4 OUTLINE_COLOR;
    
    // Outer glow / drop shadow
    bool OUTER_GLOW;
    vec2 GLOW_UV_OFFSET;
    float OUTER_GLOW_MIN_DVALUE;
    float OUTER_GLOW_MAX_DVALUE;
    vec4 OUTER_GLOW_COLOR;
    
    // Base color multiplier
    vec4 baseColorMultiplier;
} sdf;

void main() {
    // Sample the distance field (stored in alpha channel)
    vec4 baseColor = texture(baseColorTexture, v_texCoord);
    if (textureSize(baseColorTexture, 0).x == 0) {
        baseColor = vec4(1.0);
    }
    
    float distAlphaMask = texture(baseTextureSampler, v_texCoord).a;
    
    // Apply outlining (matches paper's HLSL code)
    if (sdf.OUTLINE && 
        distAlphaMask >= sdf.OUTLINE_MIN_VALUE0 && 
        distAlphaMask <= sdf.OUTLINE_MAX_VALUE1) {
        
        float oFactor = 1.0;
        
        if (distAlphaMask <= sdf.OUTLINE_MIN_VALUE1) {
            oFactor = smoothstep(
                sdf.OUTLINE_MIN_VALUE0,
                sdf.OUTLINE_MIN_VALUE1,
                distAlphaMask
            );
        } else {
            oFactor = smoothstep(
                sdf.OUTLINE_MAX_VALUE1,
                sdf.OUTLINE_MAX_VALUE0,
                distAlphaMask
            );
        }
        
        baseColor = mix(baseColor, sdf.OUTLINE_COLOR, oFactor);
    }
    
    // Apply soft edges or alpha test (matches paper's HLSL code)
    if (sdf.SOFT_EDGES) {
        baseColor.a *= smoothstep(
            sdf.SOFT_EDGE_MIN,
            sdf.SOFT_EDGE_MAX,
            distAlphaMask
        );
    } else {
        // Simple alpha test: threshold at 0.5
        baseColor.a = (distAlphaMask >= 0.5) ? 1.0 : 0.0;
    }
    
    // Apply outer glow / drop shadow (matches paper's HLSL code)
    if (sdf.OUTER_GLOW) {
        vec4 glowTexel = texture(
            baseTextureSampler,
            v_texCoord + sdf.GLOW_UV_OFFSET
        );
        
        vec4 glowc = sdf.OUTER_GLOW_COLOR * smoothstep(
            sdf.OUTER_GLOW_MIN_DVALUE,
            sdf.OUTER_GLOW_MAX_DVALUE,
            glowTexel.a
        );
        
        float mskUsed = (baseColor.a > 0.0) ? 1.0 : 0.0;
        baseColor = mix(glowc, baseColor, mskUsed);
    }
    
    fragColor = baseColor * sdf.baseColorMultiplier;
}
