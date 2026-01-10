// Signed Distance Field Vector Texture Rendering
//
// This shader implements:
// - Alpha testing with distance field threshold
// - Soft edge antialiasing
// - Outlining
// - Outer glow / drop shadows
// - Multi-channel support for sharp corners

#version 450

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// SDF texture sampler
layout(binding = 0) uniform sampler2D sdfTexture;

// Base color texture (optional - can use uniform color instead)
layout(binding = 1) uniform sampler2D baseColorTexture;

// Uniforms
layout(binding = 2) uniform SDFParams {
    // Alpha testing
    float alphaThreshold;        // Default: 0.5 (edge position)
    bool useAlphaTest;            // Use simple alpha test instead of soft edges
    
    // Soft edges / antialiasing
    bool useSoftEdges;
    float softEdgeMin;            // Distance threshold for soft edge start
    float softEdgeMax;            // Distance threshold for soft edge end
    
    // Outlining
    bool useOutline;
    float outlineMinValue0;       // Inner outline threshold
    float outlineMinValue1;       // Inner outline fade start
    float outlineMaxValue0;       // Outer outline fade end
    float outlineMaxValue1;       // Outer outline threshold
    vec4 outlineColor;            // Outline color
    
    // Outer glow / drop shadow
    bool useOuterGlow;
    vec2 glowUVOffset;            // Offset for glow/shadow lookup
    float outerGlowMinDValue;     // Minimum distance for glow
    float outerGlowMaxDValue;     // Maximum distance for glow
    vec4 outerGlowColor;          // Glow/shadow color
    
    // Base color (used if baseColorTexture is not bound)
    vec4 baseColor;
    
    // Multi-channel SDF for sharp corners
    bool useMultiChannel;         // Use multi-channel SDF (AND operation)
    int channelCount;             // Number of channels to use (1-4)
    
    // Screen-space derivatives for adaptive antialiasing
    bool useAdaptiveAA;           // Use screen-space derivatives for adaptive AA
} sdfParams;

// Default values
const float DEFAULT_ALPHA_THRESHOLD = 0.5;
const float DEFAULT_SOFT_EDGE_MIN = 0.45;
const float DEFAULT_SOFT_EDGE_MAX = 0.55;
const float DEFAULT_OUTLINE_MIN_0 = 0.48;
const float DEFAULT_OUTLINE_MIN_1 = 0.49;
const float DEFAULT_OUTLINE_MAX_0 = 0.51;
const float DEFAULT_OUTLINE_MAX_1 = 0.52;

void main() {
    // Sample the SDF texture
    vec4 sdfSample = texture(sdfTexture, v_texCoord);
    
    // Extract distance field value(s)
    float distAlphaMask = sdfSample.a;  // Primary distance field in alpha channel
    
    // Multi-channel SDF for sharp corners (AND operation)
    if (sdfParams.useMultiChannel) {
        if (sdfParams.channelCount >= 2) {
            distAlphaMask = min(distAlphaMask, sdfSample.r);
        }
        if (sdfParams.channelCount >= 3) {
            distAlphaMask = min(distAlphaMask, sdfSample.g);
        }
        if (sdfParams.channelCount >= 4) {
            distAlphaMask = min(distAlphaMask, sdfSample.b);
        }
    }
    
    // Adaptive antialiasing using screen-space derivatives
    float softEdgeMin = sdfParams.softEdgeMin;
    float softEdgeMax = sdfParams.softEdgeMax;
    
    if (sdfParams.useAdaptiveAA) {
        // Calculate texture coordinate derivatives
        vec2 texSize = vec2(textureSize(sdfTexture, 0));
        float dx = max(abs(dFdx(v_texCoord.x * texSize.x)), 0.001);
        float dy = max(abs(dFdy(v_texCoord.y * texSize.y)), 0.001);
        float dxy = max(dx, dy);
        
        // Widen soft region when texture is minified to reduce aliasing
        float scale = max(1.0 / dxy, 1.0);
        float softWidth = (softEdgeMax - softEdgeMin) * scale;
        float center = (softEdgeMin + softEdgeMax) * 0.5;
        softEdgeMin = center - softWidth * 0.5;
        softEdgeMax = center + softWidth * 0.5;
    }
    
    // Sample base color
    vec4 baseColor = sdfParams.baseColor;
    if (textureSize(baseColorTexture, 0).x > 0) {
        baseColor = texture(baseColorTexture, v_texCoord) * sdfParams.baseColor;
    }
    
    // Apply outlining
    if (sdfParams.useOutline) {
        if (distAlphaMask >= sdfParams.outlineMinValue0 && 
            distAlphaMask <= sdfParams.outlineMaxValue0) {
            
            float oFactor = 1.0;
            
            // Fade in from inner edge
            if (distAlphaMask <= sdfParams.outlineMinValue1) {
                oFactor = smoothstep(
                    sdfParams.outlineMinValue0,
                    sdfParams.outlineMinValue1,
                    distAlphaMask
                );
            }
            // Fade out to outer edge
            else {
                oFactor = smoothstep(
                    sdfParams.outlineMaxValue1,
                    sdfParams.outlineMaxValue0,
                    distAlphaMask
                );
            }
            
            baseColor = mix(baseColor, sdfParams.outlineColor, oFactor);
        }
    }
    
    // Apply soft edges / antialiasing
    if (sdfParams.useSoftEdges) {
        baseColor.a *= smoothstep(softEdgeMin, softEdgeMax, distAlphaMask);
    } else if (sdfParams.useAlphaTest) {
        // Simple alpha test (binary on/off)
        baseColor.a = (distAlphaMask >= sdfParams.alphaThreshold) ? 1.0 : 0.0;
    } else {
        // Default: use distance field directly as alpha
        baseColor.a = distAlphaMask;
    }
    
    // Apply outer glow / drop shadow
    if (sdfParams.useOuterGlow) {
        vec4 glowTexel = texture(sdfTexture, v_texCoord + sdfParams.glowUVOffset);
        float glowDist = glowTexel.a;
        
        // Multi-channel support for glow
        if (sdfParams.useMultiChannel) {
            if (sdfParams.channelCount >= 2) {
                glowDist = min(glowDist, glowTexel.r);
            }
            if (sdfParams.channelCount >= 3) {
                glowDist = min(glowDist, glowTexel.g);
            }
            if (sdfParams.channelCount >= 4) {
                glowDist = min(glowDist, glowTexel.b);
            }
        }
        
        float glowFactor = smoothstep(
            sdfParams.outerGlowMinDValue,
            sdfParams.outerGlowMaxDValue,
            glowDist
        );
        
        vec4 glowColor = sdfParams.outerGlowColor * glowFactor;
        
        // Blend glow behind the main shape
        float maskUsed = (baseColor.a > 0.0) ? 1.0 : 0.0;
        baseColor = mix(glowColor, baseColor, maskUsed);
    }
    
    fragColor = baseColor;
}
