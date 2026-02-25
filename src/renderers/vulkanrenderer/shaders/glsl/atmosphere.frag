#version 450

/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * This file is original work by Gopex LLC and is not derived from
 * existing id Tech 3 / ioquake3 code.
 * The engine framework is based on id Tech 3 (GPLv2).
 *
 * Atmospheric scattering fragment shader (Rayleigh + Mie).
 * Implements single-scattering atmospheric model for physically-based
 * sky rendering. Based on the Nishita/Preetham sky model with
 * Henyey-Greenstein phase function for Mie scattering.
 */

layout(set = 0, binding = 0) uniform sampler2D depthTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform AtmospherePC {
	vec4  sunDirection;
	vec4  sunColor;
	vec4  rayleighCoeffs;
	vec4  mieParams;
	vec4  atmosphereParams;
	vec4  viewOrigin;
} atm;

const float PI = 3.14159265359;
const float PLANET_RADIUS = 6371000.0;
const float ATMOSPHERE_RADIUS = 6471000.0;
const int   SCATTER_STEPS = 16;
const int   OPTICAL_DEPTH_STEPS = 8;

float rayleighPhase(float cosTheta) {
	return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta, float g) {
	float g2 = g * g;
	float num = (1.0 - g2);
	float denom = 4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
	return num / max(denom, 0.0001);
}

vec2 raySphereIntersect(vec3 origin, vec3 dir, float radius) {
	float b = dot(origin, dir);
	float c = dot(origin, origin) - radius * radius;
	float d = b * b - c;
	if (d < 0.0) return vec2(-1.0);
	d = sqrt(d);
	return vec2(-b - d, -b + d);
}

float opticalDepth(vec3 origin, vec3 dir, float rayLength, float scaleHeight) {
	float stepSize = rayLength / float(OPTICAL_DEPTH_STEPS);
	float depth = 0.0;

	for (int i = 0; i < OPTICAL_DEPTH_STEPS; i++) {
		vec3 pos = origin + dir * (float(i) + 0.5) * stepSize;
		float altitude = length(pos) - PLANET_RADIUS;
		depth += exp(-altitude / scaleHeight) * stepSize;
	}

	return depth;
}

void main() {
	vec2 uv = frag_tex_coord * 2.0 - 1.0;
	vec3 rayDir = normalize(vec3(uv.x, uv.y, -1.0));

	float rayleighScale = atm.rayleighCoeffs.w;
	float mieScale = atm.mieParams.x;
	float mieG = atm.mieParams.y;
	float sunIntensity = atm.sunColor.w;
	float rayleighHeight = atm.atmosphereParams.x;
	float mieHeight = atm.atmosphereParams.y;

	vec3 rayleighBeta = atm.rayleighCoeffs.xyz;
	vec3 mieBeta = vec3(mieScale);
	vec3 sunDir = normalize(atm.sunDirection.xyz);

	vec3 origin = vec3(0.0, PLANET_RADIUS + 1.0, 0.0);

	vec2 atmoHit = raySphereIntersect(origin, rayDir, ATMOSPHERE_RADIUS);
	if (atmoHit.y < 0.0) {
		out_color = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	float rayLength = atmoHit.y;
	vec2 planetHit = raySphereIntersect(origin, rayDir, PLANET_RADIUS);
	if (planetHit.x > 0.0) {
		rayLength = planetHit.x;
	}

	float stepSize = rayLength / float(SCATTER_STEPS);
	float cosTheta = dot(rayDir, sunDir);

	float phaseR = rayleighPhase(cosTheta);
	float phaseM = miePhase(cosTheta, mieG);

	vec3 totalRayleigh = vec3(0.0);
	vec3 totalMie = vec3(0.0);
	float optDepthR = 0.0;
	float optDepthM = 0.0;

	for (int i = 0; i < SCATTER_STEPS; i++) {
		vec3 pos = origin + rayDir * (float(i) + 0.5) * stepSize;
		float altitude = length(pos) - PLANET_RADIUS;

		float hr = exp(-altitude / rayleighHeight) * stepSize;
		float hm = exp(-altitude / mieHeight) * stepSize;

		optDepthR += hr;
		optDepthM += hm;

		vec2 sunHit = raySphereIntersect(pos, sunDir, ATMOSPHERE_RADIUS);
		if (sunHit.y > 0.0) {
			float sunOptR = opticalDepth(pos, sunDir, sunHit.y, rayleighHeight);
			float sunOptM = opticalDepth(pos, sunDir, sunHit.y, mieHeight);

			vec3 tau = rayleighBeta * (optDepthR + sunOptR) + mieBeta * 1.1 * (optDepthM + sunOptM);
			vec3 attenuation = exp(-tau);

			totalRayleigh += hr * attenuation;
			totalMie += hm * attenuation;
		}
	}

	vec3 color = sunIntensity * (phaseR * rayleighBeta * totalRayleigh + phaseM * mieBeta * totalMie);

	out_color = vec4(color, 1.0);
}
