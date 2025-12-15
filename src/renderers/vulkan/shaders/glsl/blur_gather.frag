#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

// Optimized blur using textureGather for better performance
// Samples 4 texels at once using textureGather instead of 4 separate texture() calls
// This reduces texture fetch overhead and improves cache utilization

layout(set = 0, binding = 0) uniform PRECISION_MEDIUMP sampler2D texture0;

layout(location = 0) in PRECISION_MEDIUMP vec2 tex_coord0;

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(constant_id = 0) const PRECISION_MEDIUMP float texoffset_x = 0.0;
layout(constant_id = 1) const PRECISION_MEDIUMP float texoffset_y = 0.0;
layout(constant_id = 2) const int use_gather = 1; // Enable textureGather optimization

// Gaussian weights: center=6/16, sides=5/16 each
const PRECISION_MEDIUMP float WEIGHT_CENTER = 0.375;  // 6.0 / 16.0
const PRECISION_MEDIUMP float WEIGHT_SIDE = 0.3125;   // 5.0 / 16.0

void main()
{
	PRECISION_MEDIUMP vec2 offset = vec2(texoffset_x, texoffset_y);
	
	if (use_gather != 0 && abs(texoffset_x) > 0.0 && abs(texoffset_y) > 0.0) {
		// Use textureGather to sample 4 texels at once
		// Gather samples at: (x-offset, y-offset), (x+offset, y-offset), (x-offset, y+offset), (x+offset, y+offset)
		PRECISION_MEDIUMP vec2 gatherCoord = tex_coord0;
		
		// Gather 4 samples around the center point
		PRECISION_MEDIUMP vec4 gatheredR = textureGather(texture0, gatherCoord, 0);
		PRECISION_MEDIUMP vec4 gatheredG = textureGather(texture0, gatherCoord, 1);
		PRECISION_MEDIUMP vec4 gatheredB = textureGather(texture0, gatherCoord, 2);
		
		// Average the 4 gathered samples
		PRECISION_MEDIUMP vec3 gathered = vec3(
			(gatheredR.x + gatheredR.y + gatheredR.z + gatheredR.w) * 0.25,
			(gatheredG.x + gatheredG.y + gatheredG.z + gatheredG.w) * 0.25,
			(gatheredB.x + gatheredB.y + gatheredB.z + gatheredB.w) * 0.25
		);
		
		// Sample center point
		PRECISION_MEDIUMP vec3 center = texture(texture0, tex_coord0).rgb;
		
		// Weighted blend: center gets more weight
		PRECISION_MEDIUMP vec3 base = center * WEIGHT_CENTER + gathered * (1.0 - WEIGHT_CENTER);
		
		out_color = vec4(base, 1.0);
	} else {
		// Fallback to standard 3-tap blur
		PRECISION_MEDIUMP vec3 center = texture(texture0, tex_coord0).rgb;
		PRECISION_MEDIUMP vec3 pos = texture(texture0, tex_coord0 + offset).rgb;
		PRECISION_MEDIUMP vec3 neg = texture(texture0, tex_coord0 - offset).rgb;
		
		PRECISION_MEDIUMP vec3 base = center * WEIGHT_CENTER + (pos + neg) * WEIGHT_SIDE;
		out_color = vec4(base, 1.0);
	}
}

