/*
=============================================================================
Vulkan Advanced Material Features Implementation

Adds support for clearcoat, anisotropy, sheen, subsurface scattering, etc.
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_material_system.h"

#ifdef USE_VULKAN

// Extended material structure for advanced features
typedef struct {
	// Base PBR
	vec3_t albedo;
	float roughness;
	float metallic;
	
	// Advanced features
	float clearcoat; // Clearcoat layer intensity
	float clearcoatRoughness; // Clearcoat roughness
	float anisotropy; // Anisotropy strength (-1 to 1)
	vec3_t anisotropyDirection; // Anisotropy direction (tangent space)
	float sheen; // Sheen intensity (for cloth)
	vec3_t sheenColor; // Sheen color tint
	float subsurfaceScattering; // SSS strength
	vec3_t subsurfaceColor; // SSS color
	vec3_t emissive; // Emissive color (HDR)
	float microfacet; // Microfacet tightening (0..1)
	float microfacetSharpness; // Microfacet exponent modifier (>0, 1=neutral)
	
	// Texture indices
	int clearcoatTextureIndex;
	int anisotropyTextureIndex;
	int sheenTextureIndex;
	int sssTextureIndex;
	int emissiveTextureIndex;
} vk_advanced_material_t;

void vk_advanced_materials_init( void )
{
	ri.Printf( PRINT_DEVELOPER, "Advanced material features initialized\n" );
}

void vk_advanced_materials_shutdown( void )
{
	// Cleanup advanced material resources
}

// Parse advanced material parameters from shader
void vk_advanced_materials_parse( void *material, const char *shaderText )
{
	vk_advanced_material_t *mat = (vk_advanced_material_t *)material;
	if ( !mat ) {
		return;
	}
	
	// TODO: real parser. For now, seed sane defaults if unset.
	(void)shaderText;
	mat->roughness = ( mat->roughness <= 0.0f ) ? 0.5f : mat->roughness;
	mat->metallic = Com_Clamp( 0.0f, 1.0f, mat->metallic );
	mat->clearcoat = Com_Clamp( 0.0f, 1.0f, mat->clearcoat );
	mat->clearcoatRoughness = Com_Clamp( 0.02f, 1.0f, mat->clearcoatRoughness <= 0.0f ? 0.3f : mat->clearcoatRoughness );
	mat->anisotropy = Com_Clamp( -1.0f, 1.0f, mat->anisotropy );
	VectorNormalizeFast( mat->anisotropyDirection );
	mat->sheen = Com_Clamp( 0.0f, 1.0f, mat->sheen );
	for ( int i = 0; i < 3; ++i ) {
		mat->sheenColor[i] = Com_Clamp( 0.0f, 1.0f, mat->sheenColor[i] <= 0.0f ? 1.0f : mat->sheenColor[i] );
		mat->subsurfaceColor[i] = Com_Clamp( 0.0f, 1.0f, mat->subsurfaceColor[i] <= 0.0f ? 1.0f : mat->subsurfaceColor[i] );
		mat->emissive[i] = Com_Clamp( 0.0f, 8.0f, mat->emissive[i] );
	}
	mat->subsurfaceScattering = Com_Clamp( 0.0f, 1.0f, mat->subsurfaceScattering );
	mat->microfacet = Com_Clamp( 0.0f, 1.0f, mat->microfacet );
	mat->microfacetSharpness = ( mat->microfacetSharpness <= 0.0f ) ? 1.0f : Com_Clamp( 0.1f, 8.0f, mat->microfacetSharpness );
}

// Update material uniform buffer with advanced parameters
void vk_advanced_materials_update_uniform( void *material, void *uniformData )
{
	vk_advanced_material_t *mat = (vk_advanced_material_t *)material;
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

