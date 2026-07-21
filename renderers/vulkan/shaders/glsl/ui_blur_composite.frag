#version 450
// Composite a blurred (linear) source into the swapchain through an antialiased
// rounded-rectangle mask. Re-encodes to display space, applies optional tint and
// opacity, and supports ui_filterDebug visualizations. Output alpha carries mask
// coverage so alpha blending replaces only the clipped region (no seams).
#extension GL_GOOGLE_include_directive : require
#include "ui_blur_common.glsl"

layout(set = 0, binding = 0) uniform sampler2D blurredTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec2 uv = uib_src_uv(frag_tex_coord);
	vec4 blurred = textureLod(blurredTex, uv, 0.0);

	int debugMode = int(pc.misc.x + 0.5);

	// Antialiased rounded-rect coverage via analytic SDF derivative.
	float sd = uib_rounded_rect_sd(gl_FragCoord.xy);
	float aa = max(fwidth(sd), 1.0e-4);
	float coverage = 1.0 - smoothstep(-aa, aa, sd);

	vec3 rgb = blurred.rgb;
	if (uib_has(UIB_FLAG_ENCODE_SRGB)) {
		rgb = uib_linear_to_srgb(rgb);
	}

	// Tint (straight alpha) over the blurred backdrop.
	if (uib_has(UIB_FLAG_APPLY_TINT) && pc.tint.a > 0.0) {
		rgb = mix(rgb, pc.tint.rgb, pc.tint.a);
	}

	float alpha = coverage * clamp(pc.params.z, 0.0, 1.0);
	// filter-layer content carries its own alpha (premultiplied by coverage).
	if (!uib_has(UIB_FLAG_APPLY_TINT)) {
		alpha *= blurred.a;
	}

	if (debugMode == 1 || debugMode == 2) {
		// Layer bounds (1) / expanded blur bounds (2): draw an outline.
		float edge = abs(sd);
		float line = 1.0 - smoothstep(0.0, 2.0, edge);
		vec3 col = (debugMode == 1) ? vec3(0.1, 1.0, 0.2) : vec3(1.0, 0.6, 0.1);
		out_color = vec4(col, line * 0.9);
		return;
	}
	if (debugMode == 3) {
		// Backdrop source region: show the (display-encoded) source, masked.
		out_color = vec4(rgb, coverage);
		return;
	}
	if (debugMode == 4) {
		// Blurred result, full (ignore mask).
		out_color = vec4(rgb, 1.0);
		return;
	}
	if (debugMode == 5) {
		// Rounded clipping mask as grayscale.
		out_color = vec4(vec3(coverage), 1.0);
		return;
	}

	out_color = vec4(rgb, alpha);
}
