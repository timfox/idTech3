#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

// Optimized 3-tap gaussian blur 
// Exploiting linear filtering with -1.2 0 +1.2 texture offsets and 5 6 5 weighting
// to emulate 5-tap blur with better precision

layout(set = 0, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture0;

layout(location = 0) in PRECISION_MEDIUMP vec2 tex_coord0;

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(constant_id = 0) const PRECISION_MEDIUMP float texoffset_x = 0.0;
layout(constant_id = 1) const PRECISION_MEDIUMP float texoffset_y = 0.0;

// Gaussian weights: center=6/16, sides=5/16 each
const PRECISION_MEDIUMP float WEIGHT_CENTER = 0.375;  // 6.0 / 16.0
const PRECISION_MEDIUMP float WEIGHT_SIDE = 0.3125;   // 5.0 / 16.0

void main()
{
	PRECISION_MEDIUMP vec2 offset = vec2(texoffset_x, texoffset_y);
	
	// Sample three taps: center, positive offset, negative offset
	PRECISION_MEDIUMP vec3 center = texture(texture0, tex_coord0).rgb;
	PRECISION_MEDIUMP vec3 pos = texture(texture0, tex_coord0 + offset).rgb;
	PRECISION_MEDIUMP vec3 neg = texture(texture0, tex_coord0 - offset).rgb;
	
	// Weighted sum
	PRECISION_MEDIUMP vec3 base = center * WEIGHT_CENTER + (pos + neg) * WEIGHT_SIDE;

	out_color = vec4(base, 1.0);
}
