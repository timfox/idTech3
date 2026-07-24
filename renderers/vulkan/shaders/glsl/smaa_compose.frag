#version 450
#extension GL_GOOGLE_include_directive : require

#include "depth_view.glsl"

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D blendTexture;
layout(set = 2, binding = 0) uniform sampler2D depthTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SMAAPush {
	float threshold;
	float localContrast;
	int maxSearchSteps;
	float corner_rounding;
	float zNear;
	float zFar;
	float depthRejectRel;
	float pad;
} pc;

void main()
{
	vec2 uv = frag_tex_coord;
	ivec2 sz = textureSize(colorTexture, 0);
	vec2 texel = vec2(1.0 / float(sz.x), 1.0 / float(sz.y));

	vec4 a;
	a.x = textureLod(blendTexture, uv + vec2( texel.x, 0.0), 0.0).w; // right
	a.y = textureLod(blendTexture, uv + vec2(0.0,  texel.y), 0.0).y; // below
	a.z = textureLod(blendTexture, uv, 0.0).z;                        // center right
	a.w = textureLod(blendTexture, uv, 0.0).x;                        // center left

	if (dot(a, vec4(1.0)) < 1e-5) {
		out_color = textureLod(colorTexture, uv, 0.0);
		return;
	}

	bool h = max(a.x, a.z) > max(a.y, a.w);

	vec4 blendingOffset = vec4(0.0);
	vec2 blendingWeight = vec2(0.0);

	if (h) {
		blendingOffset = vec4(texel.x, 0.0, -texel.x, 0.0);
		blendingWeight = vec2(a.x, a.z);
	} else {
		blendingOffset = vec4(0.0, texel.y, 0.0, -texel.y);
		blendingWeight = vec2(a.y, a.w);
	}

	float totalWeight = blendingWeight.x + blendingWeight.y;
	if (totalWeight < 1e-5) {
		out_color = textureLod(colorTexture, uv, 0.0);
		return;
	}

	/* Reject neighborhood blends across strong depth discontinuities so SMAA
	 * cannot dilate bright silhouettes into unrelated background pixels. */
	float zn = max(pc.zNear, 1e-4);
	float zf = max(pc.zFar, zn + 1e-3);
	float rejectRel = max(pc.depthRejectRel, 0.02);
	float centerView = Depth_LinearizeReversedZ(textureLod(depthTexture, uv, 0.0).r, zn, zf);
	vec2 uv0 = uv + blendingOffset.xy;
	vec2 uv1 = uv + blendingOffset.zw;
	float v0 = Depth_LinearizeReversedZ(textureLod(depthTexture, uv0, 0.0).r, zn, zf);
	float v1 = Depth_LinearizeReversedZ(textureLod(depthTexture, uv1, 0.0).r, zn, zf);
	float rel0 = abs(v0 - centerView) / max(centerView, 1e-3);
	float rel1 = abs(v1 - centerView) / max(centerView, 1e-3);
	if (rel0 > rejectRel) {
		blendingWeight.x = 0.0;
	}
	if (rel1 > rejectRel) {
		blendingWeight.y = 0.0;
	}
	totalWeight = blendingWeight.x + blendingWeight.y;
	if (totalWeight < 1e-5) {
		out_color = textureLod(colorTexture, uv, 0.0);
		return;
	}
	blendingWeight /= totalWeight;

	vec4 color = vec4(0.0);
	color += blendingWeight.x * textureLod(colorTexture, uv0, 0.0);
	color += blendingWeight.y * textureLod(colorTexture, uv1, 0.0);

	out_color = color;
}
