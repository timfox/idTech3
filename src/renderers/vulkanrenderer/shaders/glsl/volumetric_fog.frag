#version 450

/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * This file is original work by Gopex LLC and is not derived from
 * existing id Tech 3 / ioquake3 code.
 * The engine framework is based on id Tech 3 (GPLv2).
 *
 * Volumetric fog ray-march fragment shader.
 *
 * Performs ray marching from the camera through the scene depth buffer,
 * accumulating fog density via exponential height fog combined with
 * 3D noise turbulence. Uses Henyey-Greenstein phase function for
 * anisotropic light scattering and Beer-Lambert absorption.
 *
 * Modes:
 *   1 = Analytical height fog only (no ray march, fast)
 *   2 = Ray-marched volumetric fog with noise
 *   3 = Full froxel-based (sampled from 3D texture, not in this shader)
 */

layout(set = 0, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform VFogPC {
	vec4 viewOrigin;
	vec4 viewForward;
	vec4 viewRight;
	vec4 viewUp;

	mat4 invProjection;

	vec4 fogParams;     // x: density, y: heightFalloff, z: heightOffset, w: maxDistance
	vec4 fogColor;      // rgb: color, w: ambient intensity
	vec4 noiseParams;   // x: scale, y: speed, z: unused, w: unused
	vec4 windParams;    // xyz: wind direction, w: absorption
	vec4 scatterParams; // x: scatter intensity, y: anisotropy (phase g), z: temporal blend, w: mode

	float time;
	float nearPlane;
	float farPlane;
	float padding;
} vfog;

const int RAY_STEPS = 48;
const float PI = 3.14159265359;

/*
 * Hash function for procedural noise.
 */
float hash(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yzx + 19.19);
	return fract((p.x + p.y) * p.z);
}

/*
 * 3D value noise for turbulent fog density.
 */
float noise3D(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	return mix(
		mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
		    mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
		    mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y),
		f.z);
}

/*
 * Fractal Brownian Motion for turbulent density variation.
 */
float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;

	for (int i = 0; i < 4; i++) {
		value += amplitude * noise3D(p * frequency);
		amplitude *= 0.5;
		frequency *= 2.0;
	}

	return value;
}

/*
 * Henyey-Greenstein phase function for anisotropic light scattering.
 * g > 0: forward scattering (fog glows toward light)
 * g < 0: back scattering
 * g = 0: isotropic
 */
float henyeyGreenstein(float cosTheta, float g) {
	float g2 = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cosTheta;
	return (1.0 - g2) / (4.0 * PI * pow(max(denom, 0.0001), 1.5));
}

/*
 * Exponential height fog density.
 * Returns fog density at a given world-space position.
 */
float heightFogDensity(vec3 worldPos) {
	float density = vfog.fogParams.x;
	float falloff = vfog.fogParams.y;
	float offset = vfog.fogParams.z;

	float relativeHeight = worldPos.y - offset;
	float heightDensity = density * exp(-falloff * max(relativeHeight, 0.0));

	return max(heightDensity, 0.0);
}

/*
 * Reconstruct world position from depth buffer.
 */
vec3 worldFromDepth(vec2 uv, float depth) {
	vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewPos = vfog.invProjection * clipPos;
	viewPos /= viewPos.w;

	vec3 worldDir = normalize(
		vfog.viewForward.xyz * viewPos.z +
		vfog.viewRight.xyz   * viewPos.x +
		vfog.viewUp.xyz      * viewPos.y
	);

	float dist = length(viewPos.xyz);
	return vfog.viewOrigin.xyz + worldDir * dist;
}

/*
 * Linearize the depth buffer value.
 */
float linearizeDepth(float d) {
	float near = vfog.nearPlane;
	float far = vfog.farPlane;
	return near * far / (far - d * (far - near));
}

void main() {
	vec3 sceneColor = texture(colorTexture, frag_tex_coord).rgb;
	float rawDepth = texture(depthTexture, frag_tex_coord).r;

	float mode = vfog.scatterParams.w;

	if (mode < 0.5) {
		out_color = vec4(sceneColor, 1.0);
		return;
	}

	float linearDepth = linearizeDepth(rawDepth);
	float maxDist = vfog.fogParams.w;
	float sceneDistance = min(linearDepth, maxDist);

	vec3 rayDir = normalize(
		vfog.viewForward.xyz +
		(frag_tex_coord.x * 2.0 - 1.0) * vfog.viewRight.xyz +
		(frag_tex_coord.y * 2.0 - 1.0) * vfog.viewUp.xyz
	);

	if (mode < 1.5) {
		float baseDensity = heightFogDensity(vfog.viewOrigin.xyz);
		float fog = 1.0 - exp(-baseDensity * sceneDistance);
		fog = clamp(fog, 0.0, 1.0);

		vec3 fogCol = vfog.fogColor.rgb;
		vec3 result = mix(sceneColor, fogCol, fog);
		out_color = vec4(result, 1.0);
		return;
	}

	/* Phase 2: Ray-marched volumetric fog */
	float stepSize = sceneDistance / float(RAY_STEPS);
	vec3 rayPos = vfog.viewOrigin.xyz;
	vec3 rayStep = rayDir * stepSize;

	float transmittance = 1.0;
	vec3 inScatter = vec3(0.0);

	float absorption = vfog.windParams.w;
	float scatterIntensity = vfog.scatterParams.x;
	float phaseG = vfog.scatterParams.y;
	float noiseScale = vfog.noiseParams.x;
	float noiseSpeed = vfog.noiseParams.y;

	vec3 windOffset = vfog.windParams.xyz * vfog.time * noiseSpeed;

	float cosTheta = dot(rayDir, normalize(vec3(0.5, 1.0, 0.3)));
	float phase = henyeyGreenstein(cosTheta, phaseG);

	for (int i = 0; i < RAY_STEPS; i++) {
		rayPos += rayStep;

		float baseDensity = heightFogDensity(rayPos);

		float turbulence = fbm(rayPos * noiseScale + windOffset);
		float density = baseDensity * (0.5 + turbulence);

		if (density > 0.0001) {
			float extinction = (density + absorption) * stepSize;
			float stepTransmittance = exp(-extinction);

			vec3 ambient = vfog.fogColor.rgb * vfog.fogColor.w;
			vec3 scattering = scatterIntensity * phase * vfog.fogColor.rgb + ambient;
			vec3 stepScatter = scattering * density * stepSize;

			inScatter += transmittance * stepScatter;
			transmittance *= stepTransmittance;
		}

		if (transmittance < 0.001) {
			break;
		}
	}

	vec3 result = sceneColor * transmittance + inScatter;
	out_color = vec4(result, 1.0);
}
