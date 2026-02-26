#version 450

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D blendTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main()
{
    vec2 uv = frag_tex_coord;
    ivec2 sz = textureSize(colorTexture, 0);
    vec2 texel = vec2(1.0 / float(sz.x), 1.0 / float(sz.y));

    vec4 a;
    a.x = texture(blendTexture, uv + vec2( texel.x, 0.0)).w; // right  (w = bottom-right weight)
    a.y = texture(blendTexture, uv + vec2(0.0,  texel.y)).y; // below  (y = top weight)
    a.z = texture(blendTexture, uv).z;                        // center (z = right weight)
    a.w = texture(blendTexture, uv).x;                        // center (x = left weight)

    if (dot(a, vec4(1.0)) < 1e-5) {
        out_color = texture(colorTexture, uv);
        return;
    }

    bool h = max(a.x, a.z) > max(a.y, a.w);

    vec4 blendingOffset = vec4(0.0);
    vec2 blendingWeight = vec2(0.0);

    if (h) {
        blendingOffset = vec4(texel.x, 0.0, -texel.x, 0.0);
        blendingWeight = vec2(a.x, a.z);
    } else {
        blendingOffset = vec4(0.0, texel.y, 0.0, -texel.y);
        blendingWeight = vec2(a.y, a.w);
    }

    float totalWeight = blendingWeight.x + blendingWeight.y;
    if (totalWeight < 1e-5) {
        out_color = texture(colorTexture, uv);
        return;
    }
    blendingWeight /= totalWeight;

    vec4 color = vec4(0.0);
    color += blendingWeight.x * texture(colorTexture, uv + blendingOffset.xy);
    color += blendingWeight.y * texture(colorTexture, uv + blendingOffset.zw);

    out_color = color;
}
