#version 450

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D previousStageTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_edge;

const float SMAA_THRESHOLD = 0.05;
const float SMAA_LOCAL_CONTRAST_ADAPTATION = 2.0;

vec4 texelSize;

void init() {
    ivec2 sz = textureSize(colorTexture, 0);
    texelSize = vec4(1.0 / float(sz.x), 1.0 / float(sz.y), float(sz.x), float(sz.y));
}

float rgb2luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    init();
    vec2 uv = frag_tex_coord;
    vec2 texel = texelSize.xy;

    float L      = rgb2luma(texture(colorTexture, uv).rgb);
    float Lleft  = rgb2luma(texture(colorTexture, uv + vec2(-texel.x, 0.0)).rgb);
    float Ltop   = rgb2luma(texture(colorTexture, uv + vec2(0.0, -texel.y)).rgb);
    float Lright = rgb2luma(texture(colorTexture, uv + vec2( texel.x, 0.0)).rgb);
    float Lbottom= rgb2luma(texture(colorTexture, uv + vec2(0.0,  texel.y)).rgb);

    vec4 delta = abs(vec4(L) - vec4(Lleft, Ltop, Lright, Lbottom));
    vec2 edges = step(vec2(SMAA_THRESHOLD), delta.xy);

    if (dot(edges, vec2(1.0)) == 0.0) {
        discard;
    }

    float LleftLeft  = rgb2luma(texture(colorTexture, uv + vec2(-2.0 * texel.x, 0.0)).rgb);
    float LtopTop    = rgb2luma(texture(colorTexture, uv + vec2(0.0, -2.0 * texel.y)).rgb);

    vec4 delta2 = abs(vec4(Lleft, Ltop, Lleft, Ltop) - vec4(LleftLeft, LtopTop, Lright, Lbottom));
    float maxDelta = max(max(delta2.x, delta2.y), max(delta2.z, delta2.w));
    maxDelta = max(maxDelta, max(delta.z, delta.w));

    edges *= step(vec2(maxDelta * (1.0 / SMAA_LOCAL_CONTRAST_ADAPTATION)), delta.xy);

    out_edge = vec4(edges, 0.0, 1.0);
}
