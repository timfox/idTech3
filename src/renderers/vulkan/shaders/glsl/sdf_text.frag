#version 450

layout(set = 0, binding = 0) uniform sampler2D sdfAtlas;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SDFParams {
    float smoothing;
    float outlineWidth;
    float outlineR;
    float outlineG;
    float outlineB;
    float outlineA;
    float shadowOffsetX;
    float shadowOffsetY;
    float shadowSoftness;
    float shadowR;
    float shadowG;
    float shadowB;
} sdf;

void main() {
    float dist = texture(sdfAtlas, frag_tex_coord).a;

    float edgeMin = 0.5 - sdf.smoothing;
    float edgeMax = 0.5 + sdf.smoothing;
    float alpha = smoothstep(edgeMin, edgeMax, dist);

    vec4 textColor = vec4(frag_color.rgb, frag_color.a * alpha);

    /* Outline */
    if (sdf.outlineWidth > 0.0) {
        float outlineEdge = 0.5 - sdf.outlineWidth;
        float outlineAlpha = smoothstep(outlineEdge - sdf.smoothing, outlineEdge + sdf.smoothing, dist);
        vec4 outlineColor = vec4(sdf.outlineR, sdf.outlineG, sdf.outlineB, sdf.outlineA * outlineAlpha);
        textColor = mix(outlineColor, textColor, alpha);
    }

    /* Drop shadow */
    if (sdf.shadowSoftness > 0.0) {
        vec2 shadowUV = frag_tex_coord - vec2(sdf.shadowOffsetX, sdf.shadowOffsetY);
        float shadowDist = texture(sdfAtlas, shadowUV).a;
        float shadowAlpha = smoothstep(0.5 - sdf.shadowSoftness, 0.5, shadowDist);
        vec4 shadowColor = vec4(sdf.shadowR, sdf.shadowG, sdf.shadowB, shadowAlpha * 0.6);
        textColor = mix(shadowColor, textColor, textColor.a);
    }

    out_color = textColor;
}
