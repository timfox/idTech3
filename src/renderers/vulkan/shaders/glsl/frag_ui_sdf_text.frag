#version 450
/*
 * Single-texture UI SDF: matches vert_tx0 + gen_frag (vertex color) I/O.
 * reserved[0] = sdfEdgeSmooth (user / r_sdfSmoothing).
 * reserved[1] = r_sdfScreenAa: scale fwidth(dist) for resolution-independent AA (0 = off).
 * No specialization constants — vk_create_pipeline omits pSpecializationInfo for this type.
 */
layout(location = 0) centroid in vec4 frag_color0;
layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 13) in vec4 var_CurrentClip;
layout(location = 14) in vec4 var_PrevClip;

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_motion;

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	float sdfEdgeSmooth;
	float sdfScreenAa;
	float _pad[7];
} pc;

void main() {
	out_motion = vec2(0.0);
	if ( abs(var_CurrentClip.w) > 1e-6 && abs(var_PrevClip.w) > 1e-6 ) {
		vec2 currUV = var_CurrentClip.xy / var_CurrentClip.w * 0.5 + 0.5;
		vec2 prevUV = var_PrevClip.xy / var_PrevClip.w * 0.5 + 0.5;
		out_motion = currUV - prevUV;
	}

	vec4 samp = texture(texture0, frag_tex_coord0);
	float dist = max(samp.a, samp.r);
	float sm = max(pc.sdfEdgeSmooth, 1e-4);
	/* Screen-space band: fwidth(dist) widens AA when the field is magnified (Valve SDF / Alvin 2020). */
	float screenW = fwidth(dist) * max(pc.sdfScreenAa, 0.0);
	float halfBand = (pc.sdfScreenAa > 1e-6) ? max(sm, screenW) : sm;
	float edgeMin = 0.5 - halfBand;
	float edgeMax = 0.5 + halfBand;
	float cov = smoothstep(edgeMin, edgeMax, dist);
	out_color = vec4(frag_color0.rgb * cov, frag_color0.a * cov);
}
