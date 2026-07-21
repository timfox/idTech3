// Shared push-constant layout and helpers for the UI filter/backdrop-filter blur
// compositor (vk_ui_blur.c). Included by ui_blur_*.frag.
//
// Blur is performed in linear color space (requirement: no gamma-space halos).
// Sources copied from the tonemapped swapchain are sRGB-encoded and must be
// decoded on ingest; composite re-encodes to display space.

layout(push_constant) uniform UiBlurPush {
	vec4 uvScaleOffset; // xy: sample UV scale, zw: sample UV offset
	vec4 texelDir;      // xy: source texel size (1/w,1/h), zw: blur direction * step
	vec4 rect;          // panel rect in target UV: (minU, minV, maxU, maxV)
	vec4 params;        // x: cornerRadius (target px), y: rotation (rad), z: opacity, w: flags (float bitfield)
	vec4 tint;          // straight-alpha RGBA tint composited over the blurred backdrop
	vec4 misc;          // x: debugMode, y: targetW, z: targetH, w: gaussTapCount
} pc;

const uint UIB_FLAG_DECODE_SRGB = 1u;
const uint UIB_FLAG_ENCODE_SRGB = 2u;
const uint UIB_FLAG_APPLY_MASK  = 4u;
const uint UIB_FLAG_APPLY_TINT  = 8u;

uint uib_flags() { return uint(pc.params.w + 0.5); }
bool uib_has(uint bit) { return (uib_flags() & bit) != 0u; }

vec3 uib_srgb_to_linear(vec3 c) {
	bvec3 cutoff = lessThanEqual(c, vec3(0.04045));
	vec3 low = c / 12.92;
	vec3 high = pow((c + 0.055) / 1.055, vec3(2.4));
	return mix(high, low, cutoff);
}

vec3 uib_linear_to_srgb(vec3 c) {
	c = clamp(c, 0.0, 1.0);
	bvec3 cutoff = lessThanEqual(c, vec3(0.0031308));
	vec3 low = c * 12.92;
	vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(high, low, cutoff);
}

// Map a full-target [0,1] tex coord into the source sample region.
vec2 uib_src_uv(vec2 uv) {
	return uv * pc.uvScaleOffset.xy + pc.uvScaleOffset.zw;
}

// Signed distance to a rounded rectangle (positive outside), evaluated in the
// panel local frame so rotated panels clip correctly. Coordinates in target px.
float uib_rounded_rect_sd(vec2 fragPx) {
	vec2 targetSize = vec2(pc.misc.y, pc.misc.z);
	vec2 rectMinPx = pc.rect.xy * targetSize;
	vec2 rectMaxPx = pc.rect.zw * targetSize;
	vec2 centerPx = 0.5 * (rectMinPx + rectMaxPx);
	vec2 halfPx = 0.5 * (rectMaxPx - rectMinPx);

	vec2 p = fragPx - centerPx;
	float a = pc.params.y;
	if (abs(a) > 0.0001) {
		float ca = cos(-a);
		float sa = sin(-a);
		p = vec2(ca * p.x - sa * p.y, sa * p.x + ca * p.y);
	}

	float r = clamp(pc.params.x, 0.0, min(halfPx.x, halfPx.y));
	vec2 q = abs(p) - (halfPx - vec2(r));
	return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}
