/* Shared Surfel GI (GIBS) data — std430 layout matches CPU SurfelGPU. */
#ifndef SURFEL_GI_GLSL
#define SURFEL_GI_GLSL

struct Surfel {
	vec4 posRadius;     /* xyz position, w radius */
	vec4 normalConf;    /* xyz normal, w confidence */
	vec4 irradianceAge; /* xyz irradiance, w = float(age) */
	uvec4 meta;         /* x=flags, y=unused, z=unused, w=unused */
};

#define SURFEL_FLAG_ACTIVE 0x1u
#define SURFEL_FLAG_VALID  0x2u
#define SURFEL_FLAG_STALE  0x4u

/* Fixed-bucket spatial hash for resolve gather. */
#define SURFEL_HASH_CELLS  4096u
#define SURFEL_HASH_BUCKET 8u

float surfel_hash(uint x) {
	x ^= x >> 16u;
	x *= 0x7feb352du;
	x ^= x >> 15u;
	x *= 0x846ca68bu;
	x ^= x >> 16u;
	return float(x) * (1.0 / 4294967296.0);
}

uint surfel_spatial_cell(vec3 pos, float cellSize) {
	ivec3 c = ivec3(floor(pos / max(cellSize, 0.05)));
	uint h = uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(c.z) * 83492791u;
	return h % SURFEL_HASH_CELLS;
}

vec3 surfel_hemi(vec2 u, vec3 n) {
	float z = u.x;
	float r = sqrt(max(0.0, 1.0 - z * z));
	float phi = 6.28318530718 * u.y;
	vec3 local = vec3(r * cos(phi), r * sin(phi), z);
	vec3 t = abs(n.y) < 0.999 ? normalize(cross(n, vec3(0.0, 1.0, 0.0)))
	                          : normalize(cross(n, vec3(1.0, 0.0, 0.0)));
	vec3 b = cross(n, t);
	return normalize(t * local.x + b * local.y + n * local.z);
}

vec3 surfel_default_albedo() {
	return vec3(0.72, 0.70, 0.66);
}

#endif
