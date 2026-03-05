#version 450

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D previousStageTexture;

layout(push_constant) uniform SMAAParams {
    float threshold;
    float localContrast;
    int maxSearchSteps;
    int _pad;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_edge;

vec4 texelSize;

void init() {
    ivec2 sz = textureSize(colorTexture, 0);
    texelSize = vec4(1.0 / float(sz.x), 1.0 / float(sz.y), float(sz.x), float(sz.y));
}

/* sRGB luminance with HDR-safe fallback: tone down very bright values for stable edge detection */
float rgb2luma(vec3 c) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return (l > 1.0) ? (1.0 + log(l)) : l;
}

void main()
{
    init();
    vec2 uv = frag_tex_coord;
    vec2 texel = texelSize.xy;

    float L       = rgb2luma(texture(colorTexture, uv).rgb);
    float Lleft   = rgb2luma(texture(colorTexture, uv + vec2(-texel.x, 0.0)).rgb);
    float Ltop    = rgb2luma(texture(colorTexture, uv + vec2(0.0, -texel.y)).rgb);
    float Lright  = rgb2luma(texture(colorTexture, uv + vec2( texel.x, 0.0)).rgb);
    float Lbottom = rgb2luma(texture(colorTexture, uv + vec2(0.0,  texel.y)).rgb);

    vec4 delta = abs(vec4(L) - vec4(Lleft, Ltop, Lright, Lbottom));
    vec2 edges = step(vec2(pc.threshold), delta.xy);

    if (dot(edges, vec2(1.0)) == 0.0) {
        discard;
    }

    float LleftLeft  = rgb2luma(texture(colorTexture, uv + vec2(-2.0 * texel.x, 0.0)).rgb);
    float LtopTop    = rgb2luma(texture(colorTexture, uv + vec2(0.0, -2.0 * texel.y)).rgb);

    vec4 delta2 = abs(vec4(Lleft, Ltop, Lleft, Ltop) - vec4(LleftLeft, LtopTop, Lright, Lbottom));
    float maxDelta = max(max(delta2.x, delta2.y), max(delta2.z, delta2.w));
    maxDelta = max(maxDelta, max(delta.z, delta.w));

    edges *= step(vec2(maxDelta * (1.0 / pc.localContrast)), delta.xy);

    /* Corner rounding: slight attenuation at L-corners for smoother silhouettes */
    float LleftTop  = rgb2luma(texture(colorTexture, uv + vec2(-texel.x, -texel.y)).rgb);
    float LrightTop = rgb2luma(texture(colorTexture, uv + vec2( texel.x, -texel.y)).rgb);
    float LleftBot  = rgb2luma(texture(colorTexture, uv + vec2(-texel.x,  texel.y)).rgb);
    float LrightBot = rgb2luma(texture(colorTexture, uv + vec2( texel.x,  texel.y)).rgb);
    vec4 deltaC = abs(vec4(L) - vec4(LleftTop, LrightTop, LleftBot, LrightBot));
    float corner = 1.0 - 0.2 * smoothstep(pc.threshold, pc.threshold * 2.0, min(min(deltaC.x, deltaC.y), min(deltaC.z, deltaC.w)));
    edges *= corner;

    out_edge = vec4(edges, 0.0, 1.0);
}
