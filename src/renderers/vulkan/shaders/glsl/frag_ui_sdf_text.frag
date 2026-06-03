#version 450
/*
 * Single-texture UI SDF: matches vert_tx0 + gen_frag (vertex color) I/O.
 * sdfEdgeSmooth = user / r_sdfSmoothing.
 * sdfScreenAa = r_sdfScreenAa: scale fwidth(dist) for resolution-independent AA (0 = off).
 * sdfOutline / sdfOutlineWidth = Green (2007) outline ring (r_sdfOutline).
 * fontGamma = linearize coverage before display gamma (Rougier HAL-05430837).
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
	float sdfOutline;
	float sdfOutlineWidth;
	float fontGamma;
	float _pad[3];
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
	float screenW = fwidth(dist) * max(pc.sdfScreenAa, 0.0);
	float halfBand = (pc.sdfScreenAa > 1e-6) ? max(sm, screenW) : sm;
	float edgeMin = 0.5 - halfBand;
	float edgeMax = 0.5 + halfBand;
	float cov = smoothstep(edgeMin, edgeMax, dist);

	if ( pc.sdfOutline > 0.5 ) {
		float ow = max(pc.sdfOutlineWidth, 0.01);
		float outerMin = 0.5 - ow - halfBand;
		float outerMax = 0.5 - ow + halfBand;
		float outlineCov = smoothstep(outerMin, outerMax, dist) * (1.0 - cov);
		cov = clamp(cov + outlineCov, 0.0, 1.0);
	}

	if ( abs(pc.fontGamma - 1.0) > 1e-3 ) {
		float invGamma = 1.0 / max(pc.fontGamma, 0.001);
		cov = pow(clamp(cov, 0.0, 1.0), invGamma);
	}

	out_color = vec4(frag_color0.rgb, frag_color0.a * cov);
}
