/*
=============================================================================
Vulkan Advanced Material Features Implementation

Adds support for clearcoat, anisotropy, sheen, subsurface scattering, etc.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_material_system.h"

#ifdef USE_VULKAN

// Ultra-advanced material structure for next-gen rendering
typedef struct {
	// Base PBR
	vec3_t albedo;
	float roughness;
	float metallic;

	// Advanced surface properties
	float clearcoat; // Clearcoat layer intensity
	float clearcoatRoughness; // Clearcoat roughness
	float clearcoatIOR; // Clearcoat index of refraction (1.4-2.0)

	float anisotropy; // Anisotropy strength (-1 to 1)
	vec3_t anisotropyDirection; // Anisotropy direction (tangent space)
	float anisotropyRotation; // Anisotropy rotation angle

	// Cloth and fabric properties
	float sheen; // Sheen intensity (for cloth/silk)
	vec3_t sheenColor; // Sheen color tint
	float sheenRoughness; // Sheen roughness

	// Subsurface scattering
	float subsurfaceScattering; // SSS strength
	vec3_t subsurfaceColor; // SSS color
	float subsurfaceRadius; // SSS scattering radius
	float subsurfaceIOR; // SSS index of refraction

	// Advanced optical properties
	float dispersion; // Light dispersion strength (for gems)
	float iridescence; // Iridescence strength (0-1)
	float iridescenceIOR; // Iridescence index of refraction
	vec3_t iridescenceThickness; // Iridescence film thickness

	// Volumetric properties
	float transmission; // Light transmission (0-1)
	float absorption; // Light absorption coefficient
	vec3_t absorptionColor; // Absorption color
	float thickness; // Material thickness (for transmission)

	// Special effects
	vec3_t emissive; // Emissive color (HDR)
	float emissiveIntensity; // Emissive intensity multiplier

	float microfacet; // Microfacet tightening (0..1)
	float microfacetSharpness; // Microfacet exponent modifier (>0, 1=neutral)

	// Energy conservation and realism
	float specularTint; // Specular tint strength (0-1)
	float sheenTint; // Sheen tint strength (0-1)
	float specular; // Overall specular intensity

	// Advanced BRDF parameters
	float specularDistribution; // GGX vs Beckmann (0=GGX, 1=Beckmann)
	float fresnelType; // Schlick vs full Fresnel (0=Schlick, 1=full)

	// Texture indices for advanced features
	int clearcoatTextureIndex;
	int clearcoatRoughnessTextureIndex;
	int anisotropyTextureIndex;
	int sheenTextureIndex;
	int sssTextureIndex;
	int emissiveTextureIndex;
	int transmissionTextureIndex;
	int thicknessTextureIndex;
	int iridescenceTextureIndex;
	int dispersionTextureIndex;

	// Procedural texture parameters
	float proceduralScale; // Scale for procedural patterns
	float proceduralStrength; // Strength of procedural effects
	int proceduralType; // Type of procedural pattern (0=none, 1=noise, 2=marble, etc.)

	// Material classification
	qboolean isCloth; // Cloth material flag
	qboolean isMetal; // Metal material flag
	qboolean isDielectric; // Dielectric material flag
	qboolean isEmissive; // Emissive material flag
	qboolean usesTransmission; // Transmission material flag
	qboolean usesSubsurface; // Subsurface scattering flag

	// Performance optimization flags
	qboolean useSimplifiedBRDF; // Use simplified BRDF for performance
	qboolean forceOpaque; // Force opaque rendering
	float lodBias; // Level of detail bias

	// Runtime computed values (cached)
	float averageAlbedo; // Precomputed average albedo for LOD
	float energyCompensation; // Energy conservation factor
} vk_ultra_advanced_material_t;

void vk_advanced_materials_init( void )
{
	ri.Printf( PRINT_DEVELOPER, "Advanced material features initialized\n" );
}

void vk_advanced_materials_shutdown( void )
{
	// Cleanup advanced material resources
}

// Parse ultra-advanced material parameters from shader
void vk_ultra_advanced_materials_parse( void *material, const char *shaderText )
{
	vk_ultra_advanced_material_t *mat = (vk_ultra_advanced_material_t *)material;
	if ( !mat ) {
		return;
	}

	// Initialize all parameters to sensible defaults
	mat->roughness = Com_Clamp( 0.0f, 1.0f, mat->roughness <= 0.0f ? 0.5f : mat->roughness );
	mat->metallic = Com_Clamp( 0.0f, 1.0f, mat->metallic );

	// Advanced surface properties
	mat->clearcoat = Com_Clamp( 0.0f, 1.0f, mat->clearcoat );
	mat->clearcoatRoughness = Com_Clamp( 0.02f, 1.0f, mat->clearcoatRoughness <= 0.0f ? 0.3f : mat->clearcoatRoughness );
	mat->clearcoatIOR = Com_Clamp( 1.3f, 2.5f, mat->clearcoatIOR <= 0.0f ? 1.5f : mat->clearcoatIOR );

	// Anisotropy
	mat->anisotropy = Com_Clamp( -1.0f, 1.0f, mat->anisotropy );
	if ( VectorLength( mat->anisotropyDirection ) < 0.1f ) {
		VectorSet( mat->anisotropyDirection, 1.0f, 0.0f, 0.0f ); // Default tangent direction
	}
	VectorNormalizeFast( mat->anisotropyDirection );
	mat->anisotropyRotation = fmodf( mat->anisotropyRotation, 2.0f * M_PI );

	// Cloth properties
	mat->sheen = Com_Clamp( 0.0f, 1.0f, mat->sheen );
	mat->sheenRoughness = Com_Clamp( 0.0f, 1.0f, mat->sheenRoughness <= 0.0f ? 0.3f : mat->sheenRoughness );

	// Subsurface scattering
	mat->subsurfaceScattering = Com_Clamp( 0.0f, 1.0f, mat->subsurfaceScattering );
	mat->subsurfaceRadius = Com_Clamp( 0.1f, 10.0f, mat->subsurfaceRadius <= 0.0f ? 1.0f : mat->subsurfaceRadius );
	mat->subsurfaceIOR = Com_Clamp( 1.0f, 2.0f, mat->subsurfaceIOR <= 0.0f ? 1.4f : mat->subsurfaceIOR );

	// Optical properties
	mat->dispersion = Com_Clamp( 0.0f, 1.0f, mat->dispersion );
	mat->iridescence = Com_Clamp( 0.0f, 1.0f, mat->iridescence );
	mat->iridescenceIOR = Com_Clamp( 1.0f, 3.0f, mat->iridescenceIOR <= 0.0f ? 1.8f : mat->iridescenceIOR );

	// Volumetric properties
	mat->transmission = Com_Clamp( 0.0f, 1.0f, mat->transmission );
	mat->absorption = Com_Clamp( 0.0f, 10.0f, mat->absorption );
	mat->thickness = Com_Clamp( 0.0f, 10.0f, mat->thickness );

	// Special effects
	mat->emissiveIntensity = Com_Clamp( 0.0f, 100.0f, mat->emissiveIntensity );

	// Advanced BRDF parameters
	mat->specularTint = Com_Clamp( 0.0f, 1.0f, mat->specularTint );
	mat->sheenTint = Com_Clamp( 0.0f, 1.0f, mat->sheenTint );
	mat->specular = Com_Clamp( 0.0f, 2.0f, mat->specular <= 0.0f ? 0.5f : mat->specular );

	// Microfacet parameters
	mat->microfacet = Com_Clamp( 0.0f, 1.0f, mat->microfacet );
	mat->microfacetSharpness = Com_Clamp( 0.1f, 8.0f, mat->microfacetSharpness <= 0.0f ? 1.0f : mat->microfacetSharpness );

	// Color validation and normalization
	for ( int i = 0; i < 3; ++i ) {
		mat->sheenColor[i] = Com_Clamp( 0.0f, 1.0f, mat->sheenColor[i] <= 0.0f ? 1.0f : mat->sheenColor[i] );
		mat->subsurfaceColor[i] = Com_Clamp( 0.0f, 1.0f, mat->subsurfaceColor[i] <= 0.0f ? 1.0f : mat->subsurfaceColor[i] );
		mat->iridescenceThickness[i] = Com_Clamp( 0.0f, 1.0f, mat->iridescenceThickness[i] );
		mat->absorptionColor[i] = Com_Clamp( 0.0f, 1.0f, mat->absorptionColor[i] <= 0.0f ? 1.0f : mat->absorptionColor[i] );
		mat->emissive[i] = Com_Clamp( 0.0f, 8.0f, mat->emissive[i] );
	}

	// Procedural parameters
	mat->proceduralScale = Com_Clamp( 0.1f, 100.0f, mat->proceduralScale <= 0.0f ? 1.0f : mat->proceduralScale );
	mat->proceduralStrength = Com_Clamp( 0.0f, 1.0f, mat->proceduralStrength );
	mat->proceduralType = Com_Clamp( 0, 10, mat->proceduralType );

	// Material classification (auto-detect based on parameters)
	mat->isCloth = (mat->sheen > 0.1f && mat->roughness > 0.3f);
	mat->isMetal = (mat->metallic > 0.8f);
	mat->isDielectric = (mat->metallic < 0.1f && mat->transmission < 0.1f);
	mat->isEmissive = (VectorLength( mat->emissive ) > 0.1f || mat->emissiveIntensity > 0.1f);
	mat->usesTransmission = (mat->transmission > 0.01f || mat->thickness > 0.01f);
	mat->usesSubsurface = (mat->subsurfaceScattering > 0.01f);

	// Performance optimization
	mat->useSimplifiedBRDF = (mat->roughness > 0.8f && !mat->isMetal && !mat->usesTransmission);
	mat->lodBias = Com_Clamp( -2.0f, 2.0f, mat->lodBias );

	// Precompute values for performance
	mat->averageAlbedo = (mat->albedo[0] + mat->albedo[1] + mat->albedo[2]) / 3.0f;
	mat->energyCompensation = 1.0f; // Will be computed based on BRDF energy conservation

	// Parse shader text for material keywords (basic implementation)
	if ( shaderText ) {
		if ( strstr( shaderText, "clearcoat" ) ) mat->clearcoat = 1.0f;
		if ( strstr( shaderText, "cloth" ) || strstr( shaderText, "fabric" ) ) mat->isCloth = qtrue;
		if ( strstr( shaderText, "glass" ) || strstr( shaderText, "water" ) ) mat->transmission = 1.0f;
		if ( strstr( shaderText, "emissive" ) || strstr( shaderText, "glow" ) ) mat->isEmissive = qtrue;
	}

	ri.Printf( PRINT_DEVELOPER, "Ultra-advanced material parsed: metal=%.2f, rough=%.2f, clearcoat=%.2f, transmission=%.2f, emissive=%.2f\n",
	          mat->metallic, mat->roughness, mat->clearcoat, mat->transmission, mat->emissiveIntensity );
}

// Update material uniform buffer with ultra-advanced parameters
void vk_ultra_advanced_materials_update_uniform( void *material, void *uniformData )
{
	vk_ultra_advanced_material_t *mat = (vk_ultra_advanced_material_t *)material;
	material_params_t *params = (material_params_t *)uniformData;
	if ( !mat || !params ) {
		return;
	}
	
	// Pack advanced features into the material params structure.
	params->clearcoat = Com_Clamp( 0.0f, 1.0f, mat->clearcoat );
	params->clearcoatRoughness = Com_Clamp( 0.02f, 1.0f, mat->clearcoatRoughness );
	params->anisotropy = Com_Clamp( -1.0f, 1.0f, mat->anisotropy );
	VectorCopy( mat->anisotropyDirection, params->anisotropyDir );
	params->sheen = Com_Clamp( 0.0f, 1.0f, mat->sheen );
	VectorCopy( mat->sheenColor, params->sheenColor );
	params->subsurface = Com_Clamp( 0.0f, 1.0f, mat->subsurfaceScattering );
	VectorCopy( mat->subsurfaceColor, params->subsurfaceColor );
	params->microfacet = Com_Clamp( 0.0f, 1.0f, mat->microfacet );
	params->microfacetSharpness = ( mat->microfacetSharpness <= 0.0f ) ? 1.0f : Com_Clamp( 0.1f, 8.0f, mat->microfacetSharpness );
	for ( int i = 0; i < 3; ++i ) {
		params->emissive[i] += mat->emissive[i];
	}
}

#endif // USE_VULKAN

