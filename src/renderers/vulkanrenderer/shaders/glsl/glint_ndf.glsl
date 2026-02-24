/*
===============================================================================
Procedural Glint NDF helper ported from the provided reference.

The helper exposes the glint-specific D term and supporting math such as
disk mapping, UV ellipsoid handling, deterministic RNG, and compensation for
discrete sampling. All loops are bounded, determinants are clamped, and the
outputs are stable without temporal noise.
===============================================================================
*/
#ifndef GLINT_NDF_GLSL
#define GLINT_NDF_GLSL

const float GLINT_PI = 3.14159265358979323846;
const float GLINT_TWO_PI = 2.0 * GLINT_PI;

struct GlintParams {
	float density;				// actual particle density after exp(10, value) base scaling
	float microfacetRoughness;	// microfacet lattice roughness (smaller = sharper glints)
	float pixelFilterSize;		// pixel filter radius multiplier (controls uv filtering)
	float sampleBudget;			// 0=1 tap, 1=2 taps, 2=4 taps
	float maxLodClamp;			// clamp for the computed glint LOD
	float dClamp;				// clamp to prevent the final D term from spiking
};

vec2 lambert( vec3 v ) {
	return v.xy / sqrt( max( 1e-6, 1.0 + v.z ) );
}

vec3 ndf_to_disk_ggx( vec3 v, float alpha ) {
	vec3 hemi = vec3( v.xy / alpha, v.z );
	float denom = max( dot( hemi, hemi ), 1e-6 );
	vec3 disk = vec3( lambert( normalize( hemi ) ) * 0.5 + 0.5, 0.0 );
	disk.z = 1.0 / ( alpha * alpha * denom * denom );
	return disk;
}

mat2 inv_quadratic( mat2 M ) {
	float det = max( determinant( M ), 1e-12 );
	return mat2( M[1][1], -M[0][1], -M[1][0], M[0][0] ) / det;
}

mat2 uv_ellipsoid( mat2 uv_J ) {
	mat2 Q = inv_quadratic( transpose( uv_J ) );
	float tr = 0.5 * ( Q[0][0] + Q[1][1] );
	float delta = max( 0.0, tr * tr - determinant( Q ) );
	float D = sqrt( delta );
	float l1 = tr - D;
	float l2 = tr + D;
	vec2 v1 = vec2( l1 - Q[1][1], Q[0][1] );
	vec2 v2 = vec2( Q[1][0], l2 - Q[0][0] );
	vec2 n = 1.0 / sqrt( max( vec2( l1, l2 ), vec2( 1e-6 ) ) );
	return mat2( normalize( v1 ) * n.x, normalize( v2 ) * n.y );
}

float QueryLod( mat2 uv_J, float filter_size ) {
	float s0 = max( length( uv_J[0] ), 1e-6 );
	float s1 = max( length( uv_J[1] ), 1e-6 );
	float scale = max( filter_size, 0.001 );
	return max( log2( max( s0, s1 ) * scale ) + pow( 2.0, clamp( filter_size, 0.5, 2.0 ) ), 0.0 );
}

uvec2 shuffle( uvec2 v ) {
	v = v * 1664525u + 1013904223u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= ( v >> uvec2( 16u ) );
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= ( v >> uvec2( 16u ) );
	return v;
}

vec2 rand( uvec2 v ) {
	return vec2( shuffle( v ) ) * exp2( -32.0 );
}

float Rand1D( vec2 x, vec2 y, float l, uint i ) {
	uvec2 ux = floatBitsToUint( x );
	uvec2 uy = floatBitsToUint( y );
	uint ul = floatBitsToUint( l );
	return rand( ( ux >> 16u | ux << 16u ) ^ uy ^ ul ^ ( i * 0x124u ) ).x;
}

vec2 Rand2D( vec2 x, vec2 y, float l, uint i ) {
	uvec2 ux = floatBitsToUint( x );
	uvec2 uy = floatBitsToUint( y );
	uint ul = floatBitsToUint( l );
	return rand( ( ux >> 16u | ux << 16u ) ^ uy ^ ul ^ ( i * 0x124u ) );
}

float erf_approx( float x ) {
	float e = exp( -x * x );
	return sign( x ) * 2.0 * sqrt( ( 1.0 - e ) / GLINT_PI ) * ( GLINT_PI * 0.5 + 31.0 / 200.0 * e - 341.0 / 8000.0 * e * e );
}

float cdf( float x, float mu, float sigma ) {
	return 0.5 + 0.5 * erf_approx( ( x - mu ) / ( sigma * sqrt( 2.0 ) ) );
}

float integrate_interval( float x, float size, float mu, float stdev, float lower_limit, float upper_limit ) {
	return cdf( min( x + size, upper_limit ), mu, stdev ) - cdf( max( x - size, lower_limit ), mu, stdev );
}

float integrate_box( vec2 x, vec2 size, vec2 mu, mat2 sigma, vec2 lower_limit, vec2 upper_limit ) {
	return integrate_interval( x.x, size.x, mu.x, sqrt( max( sigma[0][0], 1e-8 ) ), lower_limit.x, upper_limit.x ) *
		integrate_interval( x.y, size.y, mu.y, sqrt( max( sigma[1][1], 1e-8 ) ), lower_limit.y, upper_limit.y );
}

float compensation( vec2 x_a, mat2 sigma_a, float res_a ) {
	float containing = integrate_box( vec2( 0.5 ), vec2( 0.5 ), x_a, sigma_a, vec2( 0.0 ), vec2( 1.0 ) );
	vec2 discretePos = round( x_a * res_a ) / max( res_a, 1.0 );
	float explicitly = integrate_box( discretePos, vec2( 1.0 / max( res_a, 1.0 ) ), x_a, sigma_a, vec2( 0.0 ), vec2( 1.0 ) );
	return containing - explicitly;
}

void prepareCovariance( mat2 cov, out mat2 invCov, out float invNormalization ) {
	float det = max( determinant( cov ), 1e-12 );
	invCov = mat2( cov[1][1], -cov[0][1], -cov[1][0], cov[0][0] ) / det;
	invNormalization = 1.0 / ( sqrt( det ) * GLINT_TWO_PI );
}

float evalNormal( vec2 d, mat2 invCov, float invNormalization ) {
	return exp( -0.5 * dot( d, invCov * d ) ) * invNormalization;
}

float ndf(
	vec3 h,
	float alpha,
	float glint_alpha,
	vec2 uv,
	mat2 uv_J,
	float density,
	float filter_size,
	int sampleBudget,
	float maxLodClamp,
	out float outLambda,
	out float outComp )
{
	float res = sqrt( max( density, 1.0 ) );
	vec3 x_a_and_d = ndf_to_disk_ggx( h, alpha );
	vec2 x_a = x_a_and_d.xy;
	float d = max( x_a_and_d.z, 1e-6 );

	mat2 ellipsoid = uv_ellipsoid( uv_J );
	float lambdaVal = QueryLod( res * ellipsoid, filter_size );
	float lambda = clamp( lambdaVal, 0.0, maxLodClamp );
	outLambda = lambda;

	int taps = 1 << clamp( sampleBudget, 0, 2 );
	taps = clamp( taps, 1, 4 );

	mat2 uv_J2 = filter_size * ellipsoid;
	mat2 sigma_s = uv_J2 * transpose( uv_J2 );
	mat2 invSigma_s;
	float invNorm_s;
	prepareCovariance( sigma_s, invSigma_s, invNorm_s );

	mat2 sigma_a = d * glint_alpha * glint_alpha * mat2( 1.0 );
	mat2 invSigma_a;
	float invNorm_a;
	prepareCovariance( sigma_a, invSigma_a, invNorm_a );

	float D_filter = 0.0;
	float compAccum = 0.0;

	for ( int m = 0; m < 2; ++m ) {
		float l = floor( lambda ) + float( m );
		float w_lambda = clamp( 1.0 - abs( lambda - l ), 0.0, 1.0 );
		float res_s = max( res * pow( 2.0, -l ), 1e-4 );
		float res_a = pow( 2.0, l );
		vec2 base_i_a = clamp( round( x_a * res_a ), vec2( 1.0 ), vec2( max( res_a - 1.0, 1.0 ) ) );

		for ( int j_a = 0; j_a < taps; ++j_a ) {
			vec2 i_a = base_i_a + vec2( float( j_a % 2 ), float( j_a / 2 ) ) - 0.5;

			vec2 base_i_s = round( uv * res_s );
			for ( int j_s = 0; j_s < taps; ++j_s ) {
				vec2 i_s = base_i_s + vec2( float( j_s % 2 ), float( j_s / 2 ) ) - 0.5;

				vec2 g_s = ( i_s + Rand2D( i_s, i_a, l, 1u ) - 0.5 ) / res_s;
				vec2 g_a = ( i_a + Rand2D( i_s, i_a, l, 2u ) - 0.5 ) / res_a;

				float r = Rand1D( i_s, i_a, l, 4u );
				float roulette = smoothstep( max( 0.0, r - 0.1 ), min( 1.0, r + 0.1 ), w_lambda );

				float n_a = evalNormal( x_a - g_a, invSigma_a, invNorm_a );
				float n_s = evalNormal( uv - g_s, invSigma_s, invNorm_s );

				D_filter += roulette * n_a * n_s / density;
			}
		}

		compAccum += w_lambda * compensation( x_a, sigma_a, max( res_a, 1.0 ) );
	}

	outComp = compAccum;
	return D_filter * d / GLINT_PI;
}

float D_GLINT_GGX(
	mat3 shadingFrame,
	vec3 H,
	vec2 uv,
	mat2 uv_J,
	float alpha,
	GlintParams params,
	out float outLambda,
	out float outComp )
{
	vec3 h_local = normalize( transpose( shadingFrame ) * H );
	float density = max( params.density, 1e-3 );
	float microRough = max( params.microfacetRoughness, 1e-4 );
	float pixelFilter = max( params.pixelFilterSize, 0.5 );
	float maxLod = max( params.maxLodClamp, 0.0 );
	int budget = clamp( int( floor( params.sampleBudget + 0.5 ) ), 0, 2 );
	float clampD = max( params.dClamp, 1.0 );

	float D = ndf( h_local, alpha, microRough, uv, uv_J, density, pixelFilter, budget, maxLod, outLambda, outComp );
	return min( clampD, max( 0.0, D ) );
}

#endif // GLINT_NDF_GLSL
