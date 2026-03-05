#version 450
/* OIT resolve: composite opaque background + weighted-blended transparent layers.
 * WBOIT formula: result = accum_color / max(accum_weight, 1e-5)
 * Final: blend opaque * (1 - T) + oit_result * T, T = 1 - exp(-accum_weight)
 */
layout(set = 0, binding = 0) uniform sampler2D opaqueTex;
layout(set = 1, binding = 0) uniform sampler2D oitAccumTex;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	vec3 opaque = texture(opaqueTex, frag_tex_coord).rgb;
	vec4 accum = texture(oitAccumTex, frag_tex_coord);
	vec3 oit_color = accum.rgb;
	float weight = accum.a;

	/* WBOIT resolve: oit_result = accum_color / max(weight, 1e-5) */
	vec3 oit_result = oit_color / max(weight, 1e-5);

	/* Composite: opaque behind, transparent in front. T = coverage of transparent layers */
	float T = 1.0 - exp(-weight);
	out_color = vec4(mix(opaque, oit_result, min(T, 1.0)), 1.0);
}
