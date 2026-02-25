#version 450

// SMAA compose pass
//   set 0 binding 0: resolved color input (vk.color_image_view)
//   set 1 binding 0: blend-weight output (vk.smaa_blend_image_view)
layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D blendTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

vec2 texelSize()
{
    ivec2 size = textureSize(colorTexture, 0);
    return 1.0 / max(vec2(size), vec2(1.0));
}

void main()
{
    vec2 uv = frag_tex_coord;
    vec3 center = texture(colorTexture, uv).rgb;
    vec2 weights = texture(blendTexture, uv).rg;
    vec2 texel = texelSize();

    vec3 result = center;
    float horizontalWeight = clamp(weights.x, 0.0, 1.0);
    float verticalWeight = clamp(weights.y, 0.0, 1.0);

    if (horizontalWeight > 0.01) {
        vec3 left = texture(colorTexture, uv - vec2(texel.x, 0.0)).rgb;
        vec3 right = texture(colorTexture, uv + vec2(texel.x, 0.0)).rgb;
        vec3 neighbor = (left + right) * 0.5;
        result = mix(result, neighbor, horizontalWeight);
    }

    if (verticalWeight > 0.01) {
        vec3 down = texture(colorTexture, uv - vec2(0.0, texel.y)).rgb;
        vec3 up = texture(colorTexture, uv + vec2(0.0, texel.y)).rgb;
        vec3 neighbor = (up + down) * 0.5;
        result = mix(result, neighbor, verticalWeight);
    }

    out_color = vec4(result, 1.0);
}
