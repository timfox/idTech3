#version 450
// UI blur ingest: sample a source region, optionally decode sRGB -> linear.
// Used to linearize the tonemapped scene copy and to draw filter-layer images
// into a transient target. Alpha is preserved (straight alpha).
#extension GL_GOOGLE_include_directive : require
#include "ui_blur_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D srcTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec2 uv = uib_src_uv(frag_tex_coord);
	vec4 c = textureLod(srcTex, uv, 0.0);
	if (uib_has(UIB_FLAG_DECODE_SRGB)) {
		c.rgb = uib_srgb_to_linear(c.rgb);
	}
	out_color = c;
}
