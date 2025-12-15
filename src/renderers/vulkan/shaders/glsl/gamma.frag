#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump float;
precision mediump int;

#include "shader_constants.glsl"

layout(set = 0, binding = 0) uniform mediump sampler2D texture0;

layout(location = 0) in mediump vec2 frag_tex_coord;

layout(location = 0) out mediump vec4 out_color;

layout(constant_id = 0) const mediump float gamma = 1.0;
layout(constant_id = 1) const mediump float obScale = 2.0;
layout(constant_id = 2) const mediump float greyscale = 0.0;
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;

const int BAYER_SIZE = 8;
const mediump float BAYER_MATRIX[BAYER_SIZE * BAYER_SIZE] = {
	0.0,  32.0, 8.0,  40.0, 2.0,  34.0, 10.0, 42.0,
	48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
	12.0, 44.0, 4.0,  36.0, 14.0, 46.0, 6.0,  38.0,
	60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
	3.0,  35.0, 11.0, 43.0, 1.0,  33.0, 9.0,  41.0,
	51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
	15.0, 47.0, 7.0,  39.0, 13.0, 45.0, 5.0,  37.0,
	63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
};

mediump float bayerThreshold() {
	ivec2 coord = ivec2(gl_FragCoord.xy);
	ivec2 bayerCoord = coord % BAYER_SIZE;
	int index = bayerCoord.x + bayerCoord.y * BAYER_SIZE;
	mediump float bayerSample = BAYER_MATRIX[index];
	return (bayerSample + 0.5) / float(BAYER_SIZE * BAYER_SIZE);
}

mediump vec3 dither(mediump vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	mediump vec3 cDenormalized = color * vec3(depth);
	mediump vec3 cLow = floor(cDenormalized);
	mediump vec3 cFractional = cDenormalized - cLow;
	mediump vec3 cDithered = cLow + step(bayerThreshold(), cFractional);
	return cDithered / vec3(depth);
}

void main() {
	mediump vec3 base = texture(texture0, frag_tex_coord).rgb;

	// Greyscale conversion
	if (greyscale == 1.0) {
		base = toLuma(base);
	} else if (greyscale != 0.0) {
		mediump vec3 lum = toLuma(base);
		base = mix(base, lum, greyscale);
	}

	// Gamma correction
	if (gamma != 1.0) {
		base = pow(base, vec3(gamma));
	}
	
	base *= obScale;

	// Dithering
	if (ditherMode == 1) {
		base = dither(base);
	}

	out_color = vec4(base, 1.0);
}
