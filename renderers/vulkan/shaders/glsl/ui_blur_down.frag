#version 450
// Dual-Kawase downsample (Marius Bjorge, ARM 2015). One center + four diagonal
// taps at half-texel offsets on the *source* texel size (pc.texelDir.xy).
// Used to build the downsampled blur pyramid for large radii.
#extension GL_GOOGLE_include_directive : require
#include "ui_blur_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D srcTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec2 uv = uib_src_uv(frag_tex_coord);
	vec2 hp = pc.texelDir.xy; // source half-texel already folded into caller step

	vec4 sum = textureLod(srcTex, uv, 0.0) * 4.0;
	sum += textureLod(srcTex, uv + vec2( hp.x,  hp.y), 0.0);
	sum += textureLod(srcTex, uv + vec2(-hp.x,  hp.y), 0.0);
	sum += textureLod(srcTex, uv + vec2( hp.x, -hp.y), 0.0);
	sum += textureLod(srcTex, uv + vec2(-hp.x, -hp.y), 0.0);
	out_color = sum / 8.0;
}
