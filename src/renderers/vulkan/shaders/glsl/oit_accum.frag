#version 450
/* WBOIT accumulation: output (color * w, w) with additive blend.
 * w = alpha * pow(1 - linear_depth, 2). With reversed depth, z=1 at near, 0 at far.
 * linear_depth = 1 - gl_FragCoord.z for reversed; weight favors front surfaces.
 */
/* Reversed depth: z=1 at near, z=0 at far. Higher z = closer = higher weight. */
#ifdef USE_REVERSED_DEPTH
#define DEPTH_TO_WEIGHT(z) (z)
#else
#define DEPTH_TO_WEIGHT(z) (1.0 - (z))
#endif

layout(set = 0, binding = 0) uniform sampler2D tex0;

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;

layout(location = 0) out vec4 out_color;

void main() {
	vec4 base = texture(tex0, frag_tex_coord0) * frag_color0;
	float alpha = base.a;
	if (alpha < 0.01) discard;

	float d = DEPTH_TO_WEIGHT(gl_FragCoord.z);
	float w = alpha * pow(max(d, 0.01), 2.0);
	out_color = vec4(base.rgb * w, w);
}
