#version 450
// Dual-Kawase upsample (Marius Bjorge, ARM 2015). Eight taps around the center
// on the destination texel size (pc.texelDir.xy). Progressive upsampling of the
// pyramid yields a wide, cheap blur for large radii.
#extension GL_GOOGLE_include_directive : require
#include "ui_blur_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D srcTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec2 uv = uib_src_uv(frag_tex_coord);
	vec2 hp = pc.texelDir.xy;

	vec4 sum = textureLod(srcTex, uv + vec2(-hp.x * 2.0, 0.0), 0.0);
	sum += textureLod(srcTex, uv + vec2(-hp.x, hp.y), 0.0) * 2.0;
	sum += textureLod(srcTex, uv + vec2(0.0, hp.y * 2.0), 0.0);
	sum += textureLod(srcTex, uv + vec2(hp.x, hp.y), 0.0) * 2.0;
	sum += textureLod(srcTex, uv + vec2(hp.x * 2.0, 0.0), 0.0);
	sum += textureLod(srcTex, uv + vec2(hp.x, -hp.y), 0.0) * 2.0;
	sum += textureLod(srcTex, uv + vec2(0.0, -hp.y * 2.0), 0.0);
	sum += textureLod(srcTex, uv + vec2(-hp.x, -hp.y), 0.0) * 2.0;
	out_color = sum / 12.0;
}
