// Geometry Clipmap Fragment Shader
// Implements texture LOD blending using spatial transition regions

#version 450

layout(location = 0) in vec3 v_worldPos;
layout(location = 1) in vec2 v_texCoord;
layout(location = 2) in float v_transitionAlpha;
layout(location = 3) in float v_level;

layout(location = 0) out vec4 fragColor;

// Normal maps for current and coarser level
layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D normalTextureCoarse;

// Additional textures (color, material properties, etc.)
layout(binding = 2) uniform sampler2D colorTexture;
layout(binding = 3) uniform sampler2D colorTextureCoarse;

layout(binding = 4) uniform ClipmapFragmentUniforms {
    vec3 lightDir;
    vec3 lightColor;
    vec3 ambientColor;
    float textureScale;           // Texture tiling scale
} frag;

// Blend textures using transition alpha (same as geometry morphing)
vec3 sampleNormal(vec2 texCoord, float alpha) {
    vec3 normalFine = texture(normalTexture, texCoord * frag.textureScale).rgb;
    vec3 normalCoarse = texture(normalTextureCoarse, texCoord * frag.textureScale).rgb;
    
    // Normalize after blending
    vec3 blended = mix(normalFine, normalCoarse, alpha);
    return normalize(blended * 2.0 - 1.0);  // Convert from [0,1] to [-1,1]
}

vec4 sampleColor(vec2 texCoord, float alpha) {
    vec4 colorFine = texture(colorTexture, texCoord * frag.textureScale);
    vec4 colorCoarse = texture(colorTextureCoarse, texCoord * frag.textureScale);
    return mix(colorFine, colorCoarse, alpha);
}

void main() {
    // Sample and blend textures using transition alpha
    vec3 normal = sampleNormal(v_texCoord, v_transitionAlpha);
    vec4 color = sampleColor(v_texCoord, v_transitionAlpha);
    
    // Simple lighting
    float NdotL = max(dot(normal, normalize(-frag.lightDir)), 0.0);
    vec3 lighting = frag.ambientColor + frag.lightColor * NdotL;
    
    fragColor = vec4(color.rgb * lighting, color.a);
}
