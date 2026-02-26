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
 */

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D depthTexture;
layout(set = 2, binding = 0) uniform sampler2D normalTexture;

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

void main() {
	float rawDepth = texture(depthTexture, frag_tex_coord).r;
	vec3 sceneColor = texture(colorTexture, frag_tex_coord).rgb;

	float maxDistance = ssr.params.x;
	float stepSize = ssr.params.y;
	float thickness = ssr.params.z;
	float fadeEdge = ssr.params.w;
	float roughnessThreshold = ssr.params2.x;
	float intensity = ssr.params2.y;

	if (rawDepth <= 0.0 || rawDepth >= 1.0) {
		out_color = vec4(sceneColor, 1.0);
		return;
	}

	vec3 viewPos = viewFromDepth(frag_tex_coord, rawDepth);
	vec3 normal = texture(normalTexture, frag_tex_coord).xyz * 2.0 - 1.0;

	if (length(normal) < 0.1) {
		out_color = vec4(sceneColor, 1.0);
		return;
	}
	normal = normalize(normal);

	vec3 viewDir = normalize(viewPos);
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

			hitAlpha = distFade * edgeFade * intensity;
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
