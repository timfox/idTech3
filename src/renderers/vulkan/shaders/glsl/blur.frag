#version 450

// 5-tap bilinear Gaussian blur (9-tap equivalent)
// Exploits hardware linear filtering to halve texture reads.
// Kernel: [1 8 28 56 70 56 28 8 1] / 256
//
// Bilinear tap layout:
//   center (w=0.27344)
//   +/-1.333 (w=0.32813) -- blends positions +/-1 and +/-2
//   +/-3.111 (w=0.03516) -- blends positions +/-3 and +/-4

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 tex_coord0;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float offset1_x = 0.0;
layout(constant_id = 1) const float offset1_y = 0.0;
layout(constant_id = 2) const float offset2_x = 0.0;
layout(constant_id = 3) const float offset2_y = 0.0;

void main()
{
	vec2 inner = vec2( offset1_x, offset1_y );
	vec2 outer = vec2( offset2_x, offset2_y );

	vec3 base = textureLod( texture0, tex_coord0, 0.0 ).rgb * 0.27344
		+ textureLod( texture0, tex_coord0 + inner, 0.0 ).rgb * 0.32813
		+ textureLod( texture0, tex_coord0 - inner, 0.0 ).rgb * 0.32813
		+ textureLod( texture0, tex_coord0 + outer, 0.0 ).rgb * 0.03516
		+ textureLod( texture0, tex_coord0 - outer, 0.0 ).rgb * 0.03516;

	out_color = vec4( base, 1.0 );
}
