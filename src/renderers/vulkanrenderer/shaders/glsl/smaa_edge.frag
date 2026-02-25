#version 450

layout(set = 0, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_edge;

const vec3 lumaCoeffs = vec3(0.299, 0.587, 0.114);
const float edgeThreshold = 0.08;
const float edgeKnee = 0.5;

vec2 texelSize()
{
    ivec2 size = textureSize(colorTexture, 0);
    return 1.0 / max(vec2(size), vec2(1.0));
}

float computeLuma(in vec2 uv)
{
    return dot(texture(colorTexture, uv).rgb, lumaCoeffs);
}

float detectEdge(in vec2 uv, in vec2 axis)
{
    vec2 texel = texelSize();
    float center = computeLuma(uv);
    float back = computeLuma(uv - axis * texel);
    float forward = computeLuma(uv + axis * texel);
    float delta = max(abs(center - back), abs(center - forward));
    return smoothstep(edgeThreshold * edgeKnee, edgeThreshold, delta);
}

void main()
{
    vec2 uv = frag_tex_coord;
    float edgeHorizontal = detectEdge(uv, vec2(1.0, 0.0));
    float edgeVertical = detectEdge(uv, vec2(0.0, 1.0));
    out_edge = vec4(edgeHorizontal, edgeVertical, 0.0, 1.0);
}
