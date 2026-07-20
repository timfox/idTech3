#ifndef RGI_COMMON_GLSL
#define RGI_COMMON_GLSL

const float RGI_PI = 3.14159265358979323846;

struct RgiProbe {
	vec4 posValid;   /* xyz world, w = valid*confidence (0 = invalid) */
	vec4 L0_sky;     /* rgb L0 irradiance, w = skyVis */
	vec4 L1x;        /* rgb SH L1 X, w = age (frames) */
	vec4 L1y;        /* rgb SH L1 Y, w = interior flag */
	vec4 L1z;        /* rgb SH L1 Z, w = relocate magnitude */
};

float rgi_hash_f( uint x ) {
	x ^= x >> 16u;
	x *= 0x7feb352du;
	x ^= x >> 15u;
	x *= 0x846ca68bu;
	x ^= x >> 16u;
	return float( x ) * ( 1.0 / 4294967296.0 );
}

vec3 rgi_safe_normalize( vec3 v, vec3 fallback ) {
	float len2 = dot( v, v );
	return len2 > 1e-8 ? v * inversesqrt( len2 ) : fallback;
}

/* Match Ambient Visibility depth linearization. */
vec3 rgi_view_position( vec2 uv, float depth, vec4 projInfo ) {
	float viewZ = -projInfo.w / max( depth + projInfo.z, 1e-6 );
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3( ndc.x * ( -viewZ ) * projInfo.x,
		ndc.y * ( -viewZ ) * projInfo.y, viewZ );
}

vec3 rgi_world_normal( vec3 sampled, uint normalsAreWorld, mat4 invView ) {
	vec3 n = rgi_safe_normalize( sampled, vec3( 0.0, 0.0, 1.0 ) );
	return normalsAreWorld != 0u ? n :
		rgi_safe_normalize( ( invView * vec4( n, 0.0 ) ).xyz, vec3( 0.0, 0.0, 1.0 ) );
}

vec3 rgi_eval_probe( RgiProbe p, vec3 N ) {
	vec3 L0 = p.L0_sky.rgb;
	vec3 L1 = p.L1x.rgb * N.x + p.L1y.rgb * N.y + p.L1z.rgb * N.z;
	return max( L0 + L1, vec3( 0.0 ) );
}

bool rgi_valid_depth( float d ) {
	return d > 0.0 && d < 0.999999;
}

mat3 rgi_tangent_frame( vec3 n ) {
	vec3 t = ( abs( n.z ) < 0.999 ) ? normalize( cross( vec3( 0.0, 0.0, 1.0 ), n ) )
		: normalize( cross( vec3( 0.0, 1.0, 0.0 ), n ) );
	return mat3( t, cross( n, t ), n );
}

vec3 rgi_cosine_hemisphere( vec2 u, vec3 n ) {
	float r = sqrt( clamp( u.x, 0.0, 1.0 ) );
	float phi = 2.0 * RGI_PI * u.y;
	vec3 localDir = vec3( r * cos( phi ), r * sin( phi ), sqrt( max( 0.0, 1.0 - u.x ) ) );
	return rgi_safe_normalize( rgi_tangent_frame( n ) * localDir, n );
}

#endif
