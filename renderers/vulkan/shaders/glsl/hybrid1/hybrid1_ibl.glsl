/* Prefiltered / irradiance cubemap helpers for Hybrid1 RT secondary bounces. */

const float HYBRID1_PREFILTER_MAX_LOD = 7.0;

vec3 hybrid1_samplePrefilter( samplerCube prefilterTex, vec3 dir, float roughness )
{
	float lod = clamp( roughness, 0.0, 1.0 ) * HYBRID1_PREFILTER_MAX_LOD;
	return textureLod( prefilterTex, normalize( dir ), lod ).rgb;
}

vec3 hybrid1_sampleIrradiance( samplerCube irradianceTex, vec3 dir )
{
	return texture( irradianceTex, normalize( dir ) ).rgb;
}

/* Split-sum EnvBRDF from engine BRDF LUT (NdotV, roughness). */
vec2 hybrid1_sampleEnvBRDF( sampler2D brdfLut, float NdotV, float roughness )
{
	return texture( brdfLut, vec2( clamp( NdotV, 0.0, 1.0 ), clamp( 1.0 - roughness, 0.0, 1.0 ) ) ).rg;
}
