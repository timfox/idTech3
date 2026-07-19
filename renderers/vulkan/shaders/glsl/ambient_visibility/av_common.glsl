#ifndef AMBIENT_VISIBILITY_COMMON_GLSL
#define AMBIENT_VISIBILITY_COMMON_GLSL

const float AV_PI = 3.14159265358979323846;

bool av_valid_depth(float d) {
	return d > 0.0 && d < 0.999999;
}

vec3 av_safe_normalize(vec3 v, vec3 fallbackDir) {
	float l2 = dot(v, v);
	return l2 > 1e-10 ? v * inversesqrt(l2) : fallbackDir;
}

vec2 av_oct_encode(vec3 n) {
	n /= max(abs(n.x) + abs(n.y) + abs(n.z), 1e-6);
	vec2 e = n.xy;
	if (n.z < 0.0) {
		e = (1.0 - abs(e.yx)) * sign(e.xy + vec2(1e-7));
	}
	return e * 0.5 + 0.5;
}

vec3 av_oct_decode(vec2 e) {
	vec2 f = e * 2.0 - 1.0;
	vec3 n = vec3(f, 1.0 - abs(f.x) - abs(f.y));
	if (n.z < 0.0) {
		n.xy = (1.0 - abs(n.yx)) * sign(n.xy + vec2(1e-7));
	}
	return av_safe_normalize(n, vec3(0.0, 0.0, 1.0));
}

uint av_hash(uint x) {
	x ^= x >> 16u;
	x *= 0x7feb352du;
	x ^= x >> 15u;
	x *= 0x846ca68bu;
	return x ^ (x >> 16u);
}

float av_u01(uint x) {
	return float(av_hash(x) >> 8u) * (1.0 / 16777216.0);
}

/* Low-discrepancy base-2 radical inverse with an Owen-style hash scramble. */
float av_radical_inverse(uint bits, uint scramble) {
	bits = bitfieldReverse(bits);
	bits ^= av_hash(scramble ^ bits);
	return float(bits) * 2.3283064365386963e-10;
}

vec2 av_sample_2d(uint sampleIndex, uint scramble) {
	float u = (float(sampleIndex) + av_u01(scramble)) * 0.6180339887498948;
	return vec2(fract(u), av_radical_inverse(sampleIndex, scramble));
}

mat3 av_tangent_frame(vec3 n) {
	vec3 t = (abs(n.z) < 0.999) ? normalize(cross(vec3(0.0, 0.0, 1.0), n))
		: normalize(cross(vec3(0.0, 1.0, 0.0), n));
	return mat3(t, cross(n, t), n);
}

vec3 av_cosine_hemisphere(vec2 u, vec3 n) {
	float r = sqrt(clamp(u.x, 0.0, 1.0));
	float phi = 2.0 * AV_PI * u.y;
	vec3 localDir = vec3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u.x)));
	return av_safe_normalize(av_tangent_frame(n) * localDir, n);
}

vec3 av_view_position(vec2 uv, float depth, vec4 projInfo) {
	float viewZ = -projInfo.w / max(depth + projInfo.z, 1e-6);
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3(ndc.x * (-viewZ) * projInfo.x,
		ndc.y * (-viewZ) * projInfo.y, viewZ);
}

vec3 av_world_normal(vec3 sampledNormal, uint normalsAreWorld, mat4 invView) {
	vec3 n = av_safe_normalize(sampledNormal, vec3(0.0, 0.0, 1.0));
	return normalsAreWorld != 0u ? n :
		av_safe_normalize((invView * vec4(n, 0.0)).xyz, vec3(0.0, 0.0, 1.0));
}

float av_finite_radius_visibility(float hitDistance, float radius) {
	float x = clamp(hitDistance / max(radius, 1e-4), 0.0, 1.0);
	/* C1 finite-support falloff: close blockers occlude; blockers at radius do not. */
	return x * x * (3.0 - 2.0 * x);
}

#endif
