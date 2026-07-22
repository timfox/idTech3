/* Shared PBR microfacet core (Deferred + Forward+ / OIT).
 * Canonical math for Burley diffuse, GGX NDF, Smith visibility, Schlick Fresnel.
 * Paths may wrap with FpEval_* / Diffuse_* aliases — do not fork formulas.
 */
#ifndef PBR_BRDF_CORE_GLSL
#define PBR_BRDF_CORE_GLSL

#ifndef PBR_BRDF_PI
#define PBR_BRDF_PI 3.14159265358979323846
#endif

float PbrPow5( float x )
{
	float x2 = x * x;
	return x2 * x2 * x;
}

/* Disney 2012 diffuse (Fd) for direct lighting. */
vec3 PbrDiffuseBurley( vec3 diffuseColor, float NE, float NL, float LH, float roughness )
{
	float FD90 = 0.5 + 2.0 * LH * LH * roughness;
	float lightScatter = 1.0 + ( FD90 - 1.0 ) * PbrPow5( 1.0 - NL );
	float viewScatter = 1.0 + ( FD90 - 1.0 ) * PbrPow5( 1.0 - NE );
	return diffuseColor * ( 1.0 / PBR_BRDF_PI ) * lightScatter * viewScatter;
}

float PbrD_GGX( float NH, float alpha )
{
	float alphaSq = alpha * alpha;
	float d = ( NH * alphaSq - NH ) * NH + 1.0;
	return alphaSq / ( PBR_BRDF_PI * d * d );
}

/* Height-correlated Smith GGX visibility (G / (4 NL NE) form → 0.5/(λE+λL)). */
float PbrVisibilitySmithGGX( float NL, float NE, float alpha )
{
	float alphaSq = alpha * alpha;
	float lambdaE = NL * sqrt( ( -NE * alphaSq + NE ) * NE + alphaSq );
	float lambdaL = NE * sqrt( ( -NL * alphaSq + NL ) * NL + alphaSq );
	return 0.5 / max( lambdaE + lambdaL, 1e-7 );
}

vec3 PbrFresnelSchlick( float cosTheta, vec3 F0 )
{
	return F0 + ( vec3( 1.0 ) - F0 ) * PbrPow5( 1.0 - cosTheta );
}

/* Multiscatter energy compensation (simplified Kulla/Conty-style) for metals. */
vec3 PbrEnergyCompensation( vec3 F0, float roughness )
{
	float r = clamp( roughness, 0.0, 1.0 );
	float Ess = 1.0 - 0.08 * r - 0.32 * r * r; /* approx single-scatter energy */
	float Ems = max( 1.0 - Ess, 0.0 );
	return 1.0 + F0 * ( Ems / max( Ess, 1e-4 ) );
}

/*
 * Specular AA: Toksvig-style roughness inflate from normal variance.
 * variance = |dN/dx|^2 + |dN/dy|^2 (clamped). strength scales contribution.
 * geoVariance optional geometric/interpolated-normal term.
 */
float PbrSpecularAARoughness( float roughness, float variance, float geoVariance, float strength )
{
	if ( strength <= 0.0 ) {
		return roughness;
	}
	float v = min( variance + 0.35 * geoVariance, 0.5 );
	float toksvig = v / ( 1.0 + v );
	float alpha = max( roughness * roughness, 0.0004 );
	/* Geometric roughness floor: never sharper than screen-projected curvature. */
	float geoAlpha = clamp( geoVariance * 0.25, 0.0, 0.25 );
	alpha = max( alpha, geoAlpha );
	alpha = clamp( alpha + toksvig * strength, 0.0004, 1.0 );
	return clamp( sqrt( alpha ), 0.02, 1.0 );
}

/* Grazing-angle highlight stability: slight roughness inflate when N·V is small. */
float PbrGlancingRoughness( float roughness, float NV )
{
	float g = clamp( 1.0 - NV, 0.0, 1.0 );
	float boost = g * g * 0.08;
	return clamp( roughness + boost, 0.02, 1.0 );
}

#endif /* PBR_BRDF_CORE_GLSL */
