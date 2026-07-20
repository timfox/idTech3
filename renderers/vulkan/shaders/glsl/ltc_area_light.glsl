/*
 * Linearly Transformed Cosines — rectangular area light evaluation.
 * Tables: 64x64 R16G16B16A16 mat (inverse M) + amp (Fresnel/norm).
 * Based on Heitz et al. 2016; LUT packing matches three.js RectAreaLightTexturesLib.
 *
 * Expects sampler2D ltcMatTex, ltcAmpTex when USE_LTC_AREA_LIGHT is defined.
 * Without LUTs, EvalRectAreaLight falls back to a Lambert disk approximation.
 */

#ifndef LTC_AREA_LIGHT_GLSL
#define LTC_AREA_LIGHT_GLSL

const float LTC_LUT_SIZE = 64.0;
const float LTC_LUT_SCALE = ( LTC_LUT_SIZE - 1.0 ) / LTC_LUT_SIZE;
const float LTC_LUT_BIAS = 0.5 / LTC_LUT_SIZE;

vec3 ltcIntegrateEdgeVec( vec3 v1, vec3 v2 ) {
	float x = dot( v1, v2 );
	float y = abs( x );
	float a = 0.8543985 + ( 0.4965155 + 0.0145206 * y ) * y;
	float b = 3.4175940 + ( 4.1616724 + y ) * y;
	float v = a / b;
	float theta_sintheta = ( x > 0.0 )
		? v
		: 0.5 * inversesqrt( max( 1.0 - x * x, 1e-7 ) ) - v;
	return cross( v1, v2 ) * theta_sintheta;
}

#ifdef USE_LTC_AREA_LIGHT
vec3 ltcEvaluate( vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided,
	sampler2D ampTex )
{
	vec3 T1 = normalize( V - N * dot( V, N ) );
	vec3 T2 = cross( N, T1 );
	Minv = Minv * transpose( mat3( T1, T2, N ) );

	vec3 L[4];
	L[0] = Minv * ( points[0] - P );
	L[1] = Minv * ( points[1] - P );
	L[2] = Minv * ( points[2] - P );
	L[3] = Minv * ( points[3] - P );

	vec3 dir = points[0] - P;
	vec3 lightNormal = cross( points[1] - points[0], points[3] - points[0] );
	bool behind = ( dot( dir, lightNormal ) < 0.0 );

	L[0] = normalize( L[0] );
	L[1] = normalize( L[1] );
	L[2] = normalize( L[2] );
	L[3] = normalize( L[3] );

	vec3 vsum = vec3( 0.0 );
	vsum += ltcIntegrateEdgeVec( L[0], L[1] );
	vsum += ltcIntegrateEdgeVec( L[1], L[2] );
	vsum += ltcIntegrateEdgeVec( L[2], L[3] );
	vsum += ltcIntegrateEdgeVec( L[3], L[0] );

	float len = length( vsum );
	float z = vsum.z / max( len, 1e-6 );
	if ( behind ) {
		z = -z;
	}
	vec2 uv = vec2( z * 0.5 + 0.5, len );
	uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;
	float scale = texture( ampTex, uv ).w;
	float sum = len * scale;
	if ( !behind && !twoSided ) {
		sum = 0.0;
	}
	return vec3( sum );
}
#endif

/* Build rectangle corners from center + half-extent axes (world space). */
void ltcRectCorners( vec3 center, vec3 halfU, vec3 halfV, out vec3 points[4] ) {
	points[0] = center - halfU - halfV;
	points[1] = center + halfU - halfV;
	points[2] = center + halfU + halfV;
	points[3] = center - halfU + halfV;
}

/*
 * Returns diffuse + specular contribution (already * lightColor).
 * F0 / albedo / metalness / roughness are surface terms.
 */
vec3 EvalRectAreaLight(
	vec3 N, vec3 V, vec3 P,
	vec3 center, vec3 halfU, vec3 halfV,
	vec3 lightColor,
	vec3 albedo, vec3 F0, float metalness, float roughness
#ifdef USE_LTC_AREA_LIGHT
	, sampler2D matTex, sampler2D ampTex
#endif
) {
	vec3 points[4];
	ltcRectCorners( center, halfU, halfV, points );

	/* Influence gate: skip far fragments (CPU packs radius = diagonal + margin). */
	float halfDiag = length( halfU ) + length( halfV );
	if ( length( center - P ) > halfDiag * 4.0 + 1e-3 ) {
		return vec3( 0.0 );
	}

#ifdef USE_LTC_AREA_LIGHT
	float NE = clamp( dot( N, V ), 0.0, 1.0 );
	vec2 uv = vec2( clamp( roughness, 0.0, 1.0 ), sqrt( 1.0 - NE ) );
	uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;
	vec4 t1 = texture( matTex, uv );
	vec4 t2 = texture( ampTex, uv );
	mat3 Minv = mat3(
		vec3( t1.x, 0.0, t1.y ),
		vec3( 0.0, 1.0, 0.0 ),
		vec3( t1.z, 0.0, t1.w )
	);

	vec3 specFF = ltcEvaluate( N, V, P, Minv, points, false, ampTex );
	vec3 diffFF = ltcEvaluate( N, V, P, mat3( 1.0 ), points, false, ampTex );

	vec3 diffuse = diffFF * albedo * ( 1.0 - metalness );
	vec3 specular = specFF * ( F0 * t2.x + ( vec3( 1.0 ) - F0 ) * t2.y );
	return ( diffuse + specular ) * lightColor;
#else
	/* Fallback: closest-point Lambert + soft GGX when LUTs unavailable. */
	vec3 local = P - center;
	float u = clamp( dot( local, halfU ) / max( dot( halfU, halfU ), 1e-6 ), -1.0, 1.0 );
	float v = clamp( dot( local, halfV ) / max( dot( halfV, halfV ), 1e-6 ), -1.0, 1.0 );
	vec3 closest = center + halfU * u + halfV * v;
	vec3 Lw = closest - P;
	float dist = length( Lw );
	if ( dist < 1e-4 ) {
		return vec3( 0.0 );
	}
	vec3 L = Lw / dist;
	float NL = max( dot( N, L ), 0.0 );
	float area = 4.0 * length( halfU ) * length( halfV );
	float att = area / ( dist * dist + area * 0.25 );
	vec3 H = normalize( L + V );
	float NH = max( dot( N, H ), 0.0 );
	float alpha = max( roughness * roughness, 0.04 );
	float D = ( alpha * alpha ) / max( 3.14159265 * pow( NH * NH * ( alpha * alpha - 1.0 ) + 1.0, 2.0 ), 1e-6 );
	vec3 Fd = albedo * ( 1.0 - metalness ) * ( 1.0 / 3.14159265 );
	vec3 Fs = F0 * D * 0.25;
	return lightColor * ( Fd + Fs ) * ( att * NL );
#endif
}

/* True when packed light type encodes a rect area light (lc.w >= 1.5). */
bool fpLightIsArea( float typeEncoded ) {
	return typeEncoded >= 1.5;
}

#endif /* LTC_AREA_LIGHT_GLSL */
