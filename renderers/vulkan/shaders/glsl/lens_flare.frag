#version 450

/* Screen-space lens flare (Godot-shaders.com style ghosts). Additive post-pass. */

layout(push_constant) uniform LensFlarePC {
	vec2 sunPos;
	vec2 screenSize;
	float f1Strength;
	float f2Strength;
	float f3Strength;
	float lensFlareStrength;
	float sunVisible;
	vec3 tint;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float noise_float(float t, vec2 screenSize) {
	return hash12(vec2(t, 0.0) / screenSize);
}

float noise_float2(vec2 t, vec2 screenSize) {
	return hash12(t / screenSize);
}

vec3 lensFlare(vec2 uv, vec2 pos, vec2 screenSize) {
	vec2 mainv = uv - pos;
	vec2 uvd = uv * length(uv);

	float ang = atan(mainv.y, mainv.x);
	float dist = length(mainv);
	dist = pow(dist, 0.1);

	float n = noise_float2(vec2(ang * 16.0, dist * 32.0), screenSize);

	float f1 = max(0.01 - pow(length(uv + 1.2 * pos), 1.9), 0.0) * pc.f1Strength;

	float f2 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.8 * pos), 2.0)), 0.0) * 0.25;
	float f22 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.85 * pos), 2.0)), 0.0) * 0.23;
	float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.9 * pos), 2.0)), 0.0) * 0.21;

	vec2 uvx = mix(uv, uvd, -0.5);

	float f4 = max(0.01 - pow(length(uvx + 0.4 * pos), 2.4), 0.0) * pc.f2Strength;
	float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), 0.0) * (5.0 + pc.f2Strength);
	float f43 = max(0.01 - pow(length(uvx + 0.5 * pos), 2.4), 0.0) * (3.0 + pc.f2Strength);

	uvx = mix(uv, uvd, -0.4);

	float f5 = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), 0.0) * pc.f3Strength;
	float f52 = max(0.01 - pow(length(uvx + 0.4 * pos), 5.5), 0.0) * pc.f3Strength;
	float f53 = max(0.01 - pow(length(uvx + 0.6 * pos), 5.5), 0.0) * pc.f3Strength;

	uvx = mix(uv, uvd, -0.5);

	float f6 = max(0.01 - pow(length(uvx - 0.3 * pos), 1.6), 0.0) * 6.0;
	float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), 0.0) * 3.0;
	float f63 = max(0.01 - pow(length(uvx - 0.35 * pos), 1.6), 0.0) * 5.0;

	vec3 c = vec3(0.0);
	c.r += f2 + f4 + f5 + f6 + f1;
	c.g += f22 + f42 + f52 + f62 + f1;
	c.b += f23 + f43 + f53 + f63 + f1;
	c = c * pc.lensFlareStrength * (0.92 + 0.08 * n) - vec3(length(uvd) * 0.05);

	return c;
}

void main() {
	if (pc.sunVisible <= 0.0 || pc.lensFlareStrength <= 0.0) {
		out_color = vec4(0.0);
		return;
	}

	vec2 uv = frag_tex_coord;
	vec2 sunDist = pc.sunPos - 0.5;
	float sunFalloff = 1.0 - smoothstep(0.9, 1.5, length(sunDist));

	vec3 flare = pc.tint * lensFlare(uv, pc.sunPos, pc.screenSize) * sunFalloff;
	out_color = vec4(flare, 1.0);
}
