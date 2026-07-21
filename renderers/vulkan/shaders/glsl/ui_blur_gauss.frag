#version 450
// Separable Gaussian blur for small radii. Direction and per-tap step come from
// pc.texelDir.zw; tap count from pc.misc.w. Weights computed on the fly so the
// same shader serves every quantized radius (limits shader permutations).
// Operates on linear color; alpha blurred alongside for correct premultiply.
#extension GL_GOOGLE_include_directive : require
#include "ui_blur_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D srcTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec2 uv = uib_src_uv(frag_tex_coord);
	vec2 dir = pc.texelDir.zw;
	int taps = int(pc.misc.w + 0.5);
	if (taps < 1) {
		out_color = textureLod(srcTex, uv, 0.0);
		return;
	}

	// sigma chosen so the outermost tap sits near 3 sigma.
	float sigma = max(float(taps) / 3.0, 0.5);
	float twoSigma2 = 2.0 * sigma * sigma;

	vec4 sum = textureLod(srcTex, uv, 0.0);
	float wsum = 1.0;
	for (int i = 1; i <= taps; ++i) {
		float w = exp(-float(i * i) / twoSigma2);
		vec2 off = dir * float(i);
		sum += textureLod(srcTex, uv + off, 0.0) * w;
		sum += textureLod(srcTex, uv - off, 0.0) * w;
		wsum += 2.0 * w;
	}
	out_color = sum / wsum;
}
