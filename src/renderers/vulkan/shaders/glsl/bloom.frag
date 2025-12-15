#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in PRECISION_MEDIUMP vec2 frag_tex_coord;

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(constant_id = 3) const PRECISION_MEDIUMP float threshold = 0.6;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;

void main() {
	PRECISION_MEDIUMP vec3 base = texture(texture0, frag_tex_coord).rgb;
	
	// Early exit optimization: check threshold first
	PRECISION_MEDIUMP float brightness;
	
	// Calculate brightness based on extract mode
	if (extract_mode == 1) {
		// Average RGB
		brightness = (base.r + base.g + base.b) * 0.33333333;
	} else if (extract_mode == 2) {
		// Luminance
		brightness = luma(base);
	} else {
		// Max channel
		brightness = max(max(base.r, base.g), base.b);
	}
	
	// Early discard if below threshold
	if (brightness < threshold) {
		out_color = COLOR_BLACK_A;
		return;
	}
	
	// Apply modulation if enabled
	if (base_modulate != 0) {
		if (base_modulate == 1) {
			// Square modulation
			base *= base;
		} else {
			// Luminance modulation
			PRECISION_MEDIUMP float lum = luma(base);
			base *= lum;
		}
	}
	
	out_color = vec4(base, 1.0);
}
