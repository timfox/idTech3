#version 450

layout(set = 0, binding = 0) uniform sampler2D ssaoTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSAOParams {
	vec4 projInfo; // unused
	vec4 params;   // blurRadius, unused, unused, unused
	vec4 misc;     // samples, invWidth, invHeight, depthIsReversed
} pc;

void main()
{
	int radius = int(pc.params.x);
	if ( radius <= 0 ) {
		float ao = textureLod( ssaoTex, frag_tex_coord, 0.0 ).r;
		out_color = vec4( ao, ao, ao, 1.0 );
		return;
	}

	vec2 texel = vec2(pc.misc.y, pc.misc.z);
	float sum = 0.0;
	float weightSum = 0.0;

	for (int y = -4; y <= 4; ++y) {
		if (abs(y) > radius) {
			continue;
		}
		for (int x = -4; x <= 4; ++x) {
			if (abs(x) > radius) {
				continue;
			}
			float w = 1.0 - (abs(float(x)) + abs(float(y))) / (float(radius) * 2.0 + 1.0);
			float ao = textureLod( ssaoTex, frag_tex_coord + vec2( x, y ) * texel, 0.0 ).r;
			sum += ao * w;
			weightSum += w;
		}
	}

	float result = sum / max(weightSum, 1e-6);
	out_color = vec4(result, result, result, 1.0);
}
