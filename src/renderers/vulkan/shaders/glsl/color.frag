#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(constant_id = 4) const int color_mode = 0;

// Predefined colors
const PRECISION_MEDIUMP vec4 COLOR_WHITE_VEC4 = vec4(1.0, 1.0, 1.0, 1.0);
const PRECISION_MEDIUMP vec4 COLOR_GREEN_VEC4 = vec4(0.2, 1.0, 0.2, 1.0);
const PRECISION_MEDIUMP vec4 COLOR_RED_VEC4 = vec4(1.0, 0.33, 0.2, 1.0);
const PRECISION_MEDIUMP vec4 COLOR_BLACK_VEC4 = vec4(0.0, 0.0, 0.0, 1.0);

void main()
{
	// Use switch-like branching for better GPU performance
	if (color_mode == 1) {
		out_color = COLOR_WHITE_VEC4;
	} else if (color_mode == 2) {
		out_color = COLOR_GREEN_VEC4;
	} else if (color_mode == 3) {
		out_color = COLOR_RED_VEC4;
	} else {
		out_color = COLOR_BLACK_VEC4;
	}
}
