#version 450

// Composite shader for ray tracing output
// Samples HDR RT output, applies tonemapping, and outputs to main color attachment

layout(set = 0, binding = 0) uniform sampler2D rtOutput; // HDR ray tracing output
layout(set = 0, binding = 1) uniform sampler2D rasterOutput; // Raster output (fallback when RT disabled)

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const int rtEnabled = 0; // 0 = use raster, 1 = use RT (set via specialization constant)

// Include tonemapping functions from rt_helpers
vec3 tonemapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 linearToSrgb(vec3 linear) {
    vec3 s1 = linear * 12.92;
    vec3 s2 = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    return mix(s1, s2, step(vec3(0.0031308), linear));
}

void main() {
    vec3 color;
    
    if (rtEnabled == 1) {
        // Sample HDR RT output
        vec3 hdrColor = texture(rtOutput, frag_tex_coord).rgb;
        
        // Apply tonemapping to convert HDR to LDR
        color = tonemapACES(hdrColor);
        
        // Convert to sRGB for display
        color = linearToSrgb(color);
    } else {
        // Use raster output (already in LDR/sRGB)
        color = texture(rasterOutput, frag_tex_coord).rgb;
    }
    
    out_color = vec4(color, 1.0);
}

