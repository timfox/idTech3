/* Radiance Cache GI — shared layouts (spatial-hash cache + cascaded probes). */
#ifndef RCGI_GLSL
#define RCGI_GLSL

#define RCGI_GRID_RES       12u
#define RCGI_CASCADES       4u
#define RCGI_MAX_LIGHT_IDS  64u
#define RCGI_MAX_LIGHTS     64u
#define RCGI_CACHE_CELLS    32768u
#define RCGI_CACHE_PROBE    8u
#define RCGI_PROBE_TILE     8u
#define RCGI_ATLAS_W        1024u
#define RCGI_ATLAS_H        1024u

struct RcGiLight {
	vec4 posRange;
	vec4 color;
};

struct RcGiCacheCell {
	vec4 radianceAge;
	vec4 normalDist;
	uvec4 meta; /* x=flags y=probeId z=rayId w=frame */
};

#define RCGI_CACHE_VALID 0x1u
#define RCGI_CACHE_HIT   0x2u

uint rcgi_hash(uint x) {
	x ^= x >> 16u;
	x *= 0x7feb352du;
	x ^= x >> 15u;
	x *= 0x846ca68bu;
	x ^= x >> 16u;
	return x;
}

uint rcgi_spatial_hash(vec3 pos, float cellSize, float distToCam, float lodDist) {
	uint lod = uint(exp2(floor(log2(1.0 + (distToCam / max(lodDist, 1.0))))));
	float cs = cellSize * float(max(lod, 1u));
	ivec3 p = ivec3(floor(pos / max(cs, 0.01)));
	uint h = rcgi_hash(uint(lod) + rcgi_hash(uint(p.z) + rcgi_hash(uint(p.y) + rcgi_hash(uint(p.x)))));
	return h % RCGI_CACHE_CELLS;
}

vec3 rcgi_hemi(vec2 u, vec3 n) {
	float z = u.x;
	float r = sqrt(max(0.0, 1.0 - z * z));
	float phi = 6.28318530718 * u.y;
	vec3 local = vec3(r * cos(phi), r * sin(phi), z);
	vec3 t = abs(n.y) < 0.999 ? normalize(cross(n, vec3(0.0, 1.0, 0.0)))
	                          : normalize(cross(n, vec3(1.0, 0.0, 0.0)));
	vec3 b = cross(n, t);
	return normalize(t * local.x + b * local.y + n * local.z);
}

vec2 rcgi_oct_encode(vec3 n) {
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	vec2 e = n.xy;
	if (n.z < 0.0) {
		e = (1.0 - abs(e.yx)) * vec2(e.x >= 0.0 ? 1.0 : -1.0, e.y >= 0.0 ? 1.0 : -1.0);
	}
	return e * 0.5 + 0.5;
}

vec3 rcgi_oct_decode(vec2 e) {
	e = e * 2.0 - 1.0;
	vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	float t = max(-n.z, 0.0);
	n.xy += vec2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
	return normalize(n);
}

void rcgi_sh_accum(inout vec4 shR, inout vec4 shG, inout vec4 shB, vec3 dir, vec3 rad, float w) {
	float Y0 = 0.2820947918;
	float Y1 = 0.4886025119;
	vec4 basis = vec4(Y0, Y1 * dir.y, Y1 * dir.z, Y1 * dir.x);
	shR += basis * rad.r * w;
	shG += basis * rad.g * w;
	shB += basis * rad.b * w;
}

vec3 rcgi_sh_eval(vec4 shR, vec4 shG, vec4 shB, vec3 n) {
	float Y0 = 0.2820947918;
	float Y1 = 0.4886025119;
	vec4 basis = vec4(Y0, Y1 * n.y, Y1 * n.z, Y1 * n.x);
	return max(vec3(dot(shR, basis), dot(shG, basis), dot(shB, basis)), vec3(0.0));
}

vec3 rcgi_default_sky() {
	return vec3(0.55, 0.65, 0.85);
}

vec3 rcgi_default_albedo() {
	return vec3(0.72, 0.70, 0.66);
}

#endif
