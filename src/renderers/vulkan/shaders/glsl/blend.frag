#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

layout(set = 0, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture0;
layout(set = 1, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture1;
layout(set = 2, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture2;
layout(set = 3, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture3;

layout(location = 0) in PRECISION_MEDIUMP vec2 tex_coord;

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(constant_id = 4) const PRECISION_MEDIUMP float factor = 0.5;

void main()
{
	// Sample all four textures
	PRECISION_MEDIUMP vec3 base = texture(texture0, tex_coord).rgb 
	                             + texture(texture1, tex_coord).rgb 
	                             + texture(texture2, tex_coord).rgb 
	                             + texture(texture3, tex_coord).rgb;

	// Early exit if black
	PRECISION_MEDIUMP float sqrLength = dot(base, base);
	if (sqrLength < EPSILON) {
		discard;
	}

	out_color = vec4(base * factor, 0.0);
}
