#version 450

// SMAA blend-weight pass: binding 0 = resolved color, binding 1 = edge detection output.
layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D edgeTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_weight;

const vec3 lumaCoeffs = vec3(0.299, 0.587, 0.114);
const float weightScale = 1.8;

vec2 texelSize()
{
    ivec2 size = textureSize(colorTexture, 0);
    return 1.0 / max(vec2(size), vec2(1.0));
}

float computeLuma(in vec2 uv)
{
    return dot(texture(colorTexture, uv).rgb, lumaCoeffs);
}

float computeDirectionalWeight(in vec2 uv, in vec2 axis)
{
    vec2 texel = texelSize();
    float center = computeLuma(uv);
    float forward = computeLuma(uv + axis * texel);
    float backward = computeLuma(uv - axis * texel);
    float delta = abs(forward - center) + abs(backward - center);
    return clamp(smoothstep(0.0, 1.0, delta * weightScale), 0.0, 1.0);
}

void main()
{
    vec2 uv = frag_tex_coord;
    vec4 edges = texture(edgeTexture, uv);
    float horizontalEdge = edges.r;
    float verticalEdge = edges.g;

    float weightHorizontal = horizontalEdge * computeDirectionalWeight(uv, vec2(1.0, 0.0));
    float weightVertical = verticalEdge * computeDirectionalWeight(uv, vec2(0.0, 1.0));

    out_weight = vec4(weightHorizontal, weightVertical, 0.0, 1.0);
}
