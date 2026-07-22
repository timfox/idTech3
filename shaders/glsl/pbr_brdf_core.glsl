/* Shared PBR microfacet core (Deferred + Forward+).
 * Canonical math for Burley diffuse, GGX NDF, Smith visibility, Schlick Fresnel,
 * multiscatter compensation, clearcoat / sheen lobes, and specular AA.
 * Do NOT include OIT weighting here — keep order-independent transparency separate.
 */
#ifndef PBR_BRDF_CORE_GLSL
#define PBR_BRDF_CORE_GLSL

#ifndef PBR_BRDF_PI
#define PBR_BRDF_PI 3.14159265358979323846
#endif

float brdf_pow5( float x )
{
	float x2 = x * x;
	return x2 * x2 * x;
}

/* Alias used by Forward+ / deferred wrappers. */
float PbrPow5( float x )
{
	return brdf_pow5( x );
}

vec3 prepare_normal( vec3 n, vec3 fallback )
{
	float lenSq = dot( n, n );
	if ( lenSq <= 1e-8 ) {
		return normalize( fallback );
	}
	return normalize( n );
}

vec3 diffuse_burley( vec3 diffuseColor, float NE, float NL, float LH, float roughness )
{
	float FD90 = 0.5 + 2.0 * LH * LH * roughness;
	float lightScatter = 1.0 + ( FD90 - 1.0 ) * brdf_pow5( 1.0 - NL );
	float viewScatter = 1.0 + ( FD90 - 1.0 ) * brdf_pow5( 1.0 - NE );
	return diffuseColor * ( 1.0 / PBR_BRDF_PI ) * lightScatter * viewScatter;
}

float ndf_ggx_alpha( float NH, float alpha )
{
	float alphaSq = alpha * alpha;
	float d = ( NH * alphaSq - NH ) * NH + 1.0;
	return alphaSq / ( PBR_BRDF_PI * d * d );
}

float ndf_ggx( float NH, float roughness )
{
	float alpha = max( roughness * roughness, 1e-4 );
	return ndf_ggx_alpha( NH, alpha );
}

float vis_smith_ggx_alpha( float NL, float NE, float alpha )
{
	float alphaSq = alpha * alpha;
	float lambdaE = NL * sqrt( ( -NE * alphaSq + NE ) * NE + alphaSq );
	float lambdaL = NE * sqrt( ( -NL * alphaSq + NL ) * NL + alphaSq );
	return 0.5 / max( lambdaE + lambdaL, 1e-7 );
}

float vis_smith_ggx( float NL, float NE, float roughness )
{
	float alpha = max( roughness * roughness, 1e-4 );
	return vis_smith_ggx_alpha( NL, NE, alpha );
}

vec3 fresnel_schlick( float cosTheta, vec3 F0 )
{
	return F0 + ( vec3( 1.0 ) - F0 ) * brdf_pow5( 1.0 - cosTheta );
}

vec3 multiscatter_compensation( vec3 F0, float roughness )
{
	float r = clamp( roughness, 0.0, 1.0 );
	float Ess = 1.0 - 0.08 * r - 0.32 * r * r;
	float Ems = max( 1.0 - Ess, 0.0 );
	return vec3( 1.0 ) + F0 * ( Ems / max( Ess, 1e-4 ) );
}

float sheen_charlie( float NH, float roughness )
{
	float r = clamp( roughness, 0.07, 1.0 );
	float invR = 1.0 / r;
	float sin2h = max( 1.0 - NH * NH, 0.0 );
	return ( 2.0 + invR ) * pow( sin2h, invR * 0.5 ) / ( 2.0 * PBR_BRDF_PI );
}

/* Second GGX lobe (clearcoat). Returns additive specular RGB (caller scales by lightColor * NL). */
vec3 clearcoat_lobe(
	float NH,
	float NL,
	float NE,
	float VH,
	float clearcoatStrength,
	float clearcoatRoughness )
{
	float ccRough = clamp( clearcoatRoughness, 0.02, 1.0 );
	vec3 ccF0 = vec3( 0.04 );
	vec3 ccF = fresnel_schlick( VH, ccF0 );
	float ccD = ndf_ggx( NH, ccRough );
	float ccV = vis_smith_ggx( NL, NE, ccRough );
	return ccF * ccD * ccV * clearcoatStrength;
}

float toksvig_roughness( float roughness, float variance, float strength )
{
	if ( strength <= 0.0 ) {
		return roughness;
	}
	float v = min( variance, 0.5 );
	float toksvig = v / ( 1.0 + v );
	float alpha = max( roughness * roughness, 0.0004 );
	alpha = clamp( alpha + toksvig * strength, 0.0004, 1.0 );
	return clamp( sqrt( alpha ), 0.02, 1.0 );
}

float geometric_roughness( float roughness, float geoVariance )
{
	float alpha = max( roughness * roughness, 0.0004 );
	float geoAlpha = clamp( geoVariance * 0.25, 0.0, 0.25 );
	alpha = max( alpha, geoAlpha );
	return clamp( sqrt( alpha ), 0.02, 1.0 );
}

float glancing_roughness( float roughness, float NV )
{
	float g = clamp( 1.0 - NV, 0.0, 1.0 );
	return clamp( roughness + g * g * 0.08, 0.02, 1.0 );
}

/* Legacy PascalCase aliases consumed by forward_plus_light_eval / deferred_lighting_common. */
vec3 PbrDiffuseBurley( vec3 diffuseColor, float NE, float NL, float LH, float roughness )
{
	return diffuse_burley( diffuseColor, NE, NL, LH, roughness );
}

float PbrD_GGX( float NH, float alpha )
{
	return ndf_ggx_alpha( NH, alpha );
}

float PbrVisibilitySmithGGX( float NL, float NE, float alpha )
{
	return vis_smith_ggx_alpha( NL, NE, alpha );
}

vec3 PbrFresnelSchlick( float cosTheta, vec3 F0 )
{
	return fresnel_schlick( cosTheta, F0 );
}

vec3 PbrEnergyCompensation( vec3 F0, float roughness )
{
	return multiscatter_compensation( F0, roughness );
}

float PbrSpecularAARoughness( float roughness, float variance, float geoVariance, float strength )
{
	if ( strength <= 0.0 ) {
		return roughness;
	}
	float r = geometric_roughness( roughness, geoVariance );
	r = toksvig_roughness( r, min( variance + 0.35 * geoVariance, 0.5 ), strength );
	return r;
}

float PbrGlancingRoughness( float roughness, float NV )
{
	return glancing_roughness( roughness, NV );
}

#endif /* PBR_BRDF_CORE_GLSL */
