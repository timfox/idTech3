#version 450

/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * This file is original work by Gopex LLC and is not derived from
 * existing id Tech 3 / ioquake3 code.
 * The engine framework is based on id Tech 3 (GPLv2).
 *
 * Screen-Space Reflections (SSR) fragment shader.
 * Ray-marches through the depth buffer in screen space to find
 * reflection hit points. Uses hierarchical tracing with binary
 * search refinement for performance and accuracy.
 *
 * Normals are derived from depth gradients (no G-buffer required).
 *
 * r_ssr_roughnessThreshold: optional view-dependent weight (no roughness buffer here).
 * When > 0, blends toward a grazing-angle emphasis (Fresnel-like) so SSR is stronger
 * at glancing views; 0 leaves intensity unchanged (default).
 */

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D depthTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSR_PC {
	mat4 projection;
	mat4 invProjection;
	vec4 params;
	vec4 params2;
} ssr;

const int MAX_STEPS = 64;
const int BINARY_STEPS = 8;

vec3 viewFromDepth(vec2 uv, float depth) {
	vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 view = ssr.invProjection * clip;
	return view.xyz / view.w;
}

vec2 projectToScreen(vec3 viewPos) {
	vec4 clip = ssr.projection * vec4(viewPos, 1.0);
	return (clip.xy / clip.w) * 0.5 + 0.5;
}

/* Derive view-space normal from depth using central differences (no G-buffer).
 * At depth discontinuities (object edges, horizon), gradients are unreliable and
 * produce thin black/white lines. Reject pixels where neighbor depth differs too much. */
vec3 normalFromDepth(vec2 uv, vec2 invSize, float maxDepthGradient, out bool valid) {
	float d = texture(depthTexture, uv).r;
	float dx = texture(depthTexture, uv + vec2(invSize.x, 0.0)).r;
	float dy = texture(depthTexture, uv + vec2(0.0, invSize.y)).r;
	float dxm = texture(depthTexture, uv - vec2(invSize.x, 0.0)).r;
	float dym = texture(depthTexture, uv - vec2(0.0, invSize.y)).r;

	/* Skip SSR at depth edges (object silhouettes, horizon) to avoid thin line artifacts */
	float maxDiff = max(max(abs(dx - d), abs(dxm - d)), max(abs(dy - d), abs(dym - d)));
	if (maxDiff > maxDepthGradient) {
		valid = false;
		return vec3(0.0);
	}

	vec3 p = viewFromDepth(uv, d);
	vec3 px = viewFromDepth(uv + vec2(invSize.x, 0.0), dx);
	vec3 py = viewFromDepth(uv + vec2(0.0, invSize.y), dy);
	vec3 pxm = viewFromDepth(uv - vec2(invSize.x, 0.0), dxm);
	vec3 pym = viewFromDepth(uv - vec2(0.0, invSize.y), dym);

	vec3 ddx = (px - pxm) * 0.5;
	vec3 ddy = (py - pym) * 0.5;
	vec3 normal = normalize(cross(ddx, ddy));
	valid = true;
	return normal;
}

void main() {
	float rawDepth = texture(depthTexture, frag_tex_coord).r;
	vec3 sceneColor = texture(colorTexture, frag_tex_coord).rgb;

	float maxDistance = ssr.params.x;
	float stepSize = ssr.params.y;
	float thickness = ssr.params.z;
	float fadeEdge = ssr.params.w;
	float roughnessThreshold = clamp(ssr.params2.x, 0.0, 1.0);
	float intensity = ssr.params2.y;
	float maxDepthGradient = ssr.params2.z;

	if (rawDepth <= 0.0 || rawDepth >= 1.0) {
		out_color = vec4(sceneColor, 1.0);
		return;
	}

	vec3 viewPos = viewFromDepth(frag_tex_coord, rawDepth);
	vec2 invSize = vec2(1.0) / textureSize(depthTexture, 0);
	bool normalValid;
	vec3 normal = normalFromDepth(frag_tex_coord, invSize, maxDepthGradient, normalValid);

	if (!normalValid || length(normal) < 0.1) {
		out_color = vec4(sceneColor, 1.0);
		return;
	}
	normal = normalize(normal);

	vec3 viewDir = normalize(viewPos);
	/* View from surface toward camera (for grazing / Fresnel-style SSR weight). */
	float fresnelSSRWeight = 1.0;
	if (roughnessThreshold > 0.0) {
		float NdotV = clamp(dot(normal, normalize(-viewPos)), 0.0, 1.0);
		float grazing = pow(max(1.0 - NdotV, 0.0), 2.5);
		fresnelSSRWeight = mix(1.0, grazing, roughnessThreshold);
	}
	vec3 reflectDir = reflect(viewDir, normal);

	vec3 rayPos = viewPos;
	vec3 rayStep = reflectDir * stepSize;

	vec2 hitUV = vec2(-1.0);
	float hitAlpha = 0.0;

	for (int i = 0; i < MAX_STEPS; i++) {
		rayPos += rayStep;

		vec2 screenPos = projectToScreen(rayPos);
		if (screenPos.x < 0.0 || screenPos.x > 1.0 || screenPos.y < 0.0 || screenPos.y > 1.0) {
			break;
		}

		float sampleDepth = texture(depthTexture, screenPos).r;
		vec3 sampleViewPos = viewFromDepth(screenPos, sampleDepth);

		float diff = rayPos.z - sampleViewPos.z;

		if (diff > 0.0 && diff < thickness) {
			vec3 binaryPos = rayPos;
			vec3 binaryStep = rayStep * 0.5;

			for (int j = 0; j < BINARY_STEPS; j++) {
				binaryPos -= binaryStep;
				binaryStep *= 0.5;

				vec2 bUV = projectToScreen(binaryPos);
				float bDepth = texture(depthTexture, bUV).r;
				vec3 bViewPos = viewFromDepth(bUV, bDepth);

				if (binaryPos.z > bViewPos.z) {
					binaryPos += binaryStep * 2.0;
				}
			}

			hitUV = projectToScreen(binaryPos);
			float dist = length(rayPos - viewPos);
			float distFade = 1.0 - clamp(dist / maxDistance, 0.0, 1.0);

			float edgeFade = 1.0;
			vec2 edgeDist = abs(hitUV - 0.5) * 2.0;
			edgeFade *= 1.0 - smoothstep(1.0 - fadeEdge, 1.0, edgeDist.x);
			edgeFade *= 1.0 - smoothstep(1.0 - fadeEdge, 1.0, edgeDist.y);

			hitAlpha = distFade * edgeFade * intensity * fresnelSSRWeight;
			break;
		}
	}

	if (hitAlpha > 0.001 && hitUV.x >= 0.0) {
		vec3 reflColor = texture(colorTexture, hitUV).rgb;
		out_color = vec4(mix(sceneColor, reflColor, hitAlpha), 1.0);
	} else {
		out_color = vec4(sceneColor, 1.0);
	}
}
