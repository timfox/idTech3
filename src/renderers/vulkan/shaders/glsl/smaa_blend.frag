#version 450

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D edgeTexture;

layout(push_constant) uniform SMAAParams {
    float threshold;
    float localContrast;
    int maxSearchSteps;
    int _pad;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_weight;

vec2 texelSize;

void init() {
    ivec2 sz = textureSize(colorTexture, 0);
    texelSize = vec2(1.0 / float(sz.x), 1.0 / float(sz.y));
}

float searchXLeft(vec2 uv) {
    vec2 e = vec2(0.0, 1.0);
    int maxSteps = pc.maxSearchSteps;
    for (int i = 0; i < 32; i++) {
        if (i >= maxSteps) break;
        e = texture(edgeTexture, uv).rg;
        uv -= vec2(2.0 * texelSize.x, 0.0);
        if (e.r < 0.5 || e.g > 0.5) break;
    }
    return -(2.0 * float(maxSteps)) + 2.0 * (uv.x / texelSize.x);
}

float searchXRight(vec2 uv) {
    vec2 e = vec2(0.0, 1.0);
    int maxSteps = pc.maxSearchSteps;
    for (int i = 0; i < 32; i++) {
        if (i >= maxSteps) break;
        e = texture(edgeTexture, uv).rg;
        uv += vec2(2.0 * texelSize.x, 0.0);
        if (e.r < 0.5 || e.g > 0.5) break;
    }
    return 2.0 * float(maxSteps) - 2.0 + 2.0 * (uv.x / texelSize.x);
}

float searchYUp(vec2 uv) {
    vec2 e = vec2(1.0, 0.0);
    int maxSteps = pc.maxSearchSteps;
    for (int i = 0; i < 32; i++) {
        if (i >= maxSteps) break;
        e = texture(edgeTexture, uv).rg;
        uv -= vec2(0.0, 2.0 * texelSize.y);
        if (e.g < 0.5 || e.r > 0.5) break;
    }
    return -(2.0 * float(maxSteps)) + 2.0 * (uv.y / texelSize.y);
}

float searchYDown(vec2 uv) {
    vec2 e = vec2(1.0, 0.0);
    int maxSteps = pc.maxSearchSteps;
    for (int i = 0; i < 32; i++) {
        if (i >= maxSteps) break;
        e = texture(edgeTexture, uv).rg;
        uv += vec2(0.0, 2.0 * texelSize.y);
        if (e.g < 0.5 || e.r > 0.5) break;
    }
    return 2.0 * float(maxSteps) - 2.0 + 2.0 * (uv.y / texelSize.y);
}

float computeAreaWeight(float d1, float d2) {
    float totalLen = d1 + d2;
    if (totalLen < 1.0) return 0.0;
    float t = d1 / totalLen;
    return smoothstep(0.0, 1.0, 1.0 - abs(2.0 * t - 1.0)) * clamp(totalLen * 0.125, 0.0, 1.0);
}

void main()
{
    init();
    vec2 uv = frag_tex_coord;
    vec4 weights = vec4(0.0);

    vec2 e = texture(edgeTexture, uv).rg;

    if (e.g > 0.5) {
        float startX = uv.x / texelSize.x;
        float dLeft  = startX - searchXLeft(uv - vec2(texelSize.x * 0.5, 0.0));
        float dRight = searchXRight(uv + vec2(texelSize.x * 0.5, 0.0)) - startX;

        float eLeft  = texture(edgeTexture, uv - vec2((dLeft  - 0.5) * texelSize.x, 0.0)).r;
        float eRight = texture(edgeTexture, uv + vec2((dRight + 0.5) * texelSize.x, 0.0)).r;

        float w = computeAreaWeight(dLeft, dRight);
        weights.x = w * eLeft;
        weights.z = w * eRight;
    }

    if (e.r > 0.5) {
        float startY = uv.y / texelSize.y;
        float dUp   = startY - searchYUp(uv - vec2(0.0, texelSize.y * 0.5));
        float dDown = searchYDown(uv + vec2(0.0, texelSize.y * 0.5)) - startY;

        float eUp   = texture(edgeTexture, uv - vec2(0.0, (dUp   - 0.5) * texelSize.y)).g;
        float eDown = texture(edgeTexture, uv + vec2(0.0, (dDown + 0.5) * texelSize.y)).g;

        float w = computeAreaWeight(dUp, dDown);
        weights.y = w * eUp;
        weights.w = w * eDown;
    }

    out_weight = weights;
}
