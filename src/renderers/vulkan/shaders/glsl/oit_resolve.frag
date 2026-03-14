#version 450
/* OIT resolve: composite opaque background + weighted-blended transparent layers.
 * RT0 stores weighted color / alpha, RT1 stores revealage = product(1 - alpha).
 */
layout(set = 0, binding = 0) uniform sampler2D opaqueTex;
layout(set = 1, binding = 0) uniform sampler2D oitAccumTex;
layout(set = 2, binding = 0) uniform sampler2D oitRevealTex;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	vec3 opaque = textureLod(opaqueTex, frag_tex_coord, 0.0).rgb;
	vec4 accum = textureLod(oitAccumTex, frag_tex_coord, 0.0);
	float revealage = clamp(textureLod(oitRevealTex, frag_tex_coord, 0.0).r, 0.0, 1.0);
	vec3 oit_result = accum.rgb / max(accum.a, 1e-5);
	out_color = vec4(oit_result + opaque * revealage, 1.0);
}
