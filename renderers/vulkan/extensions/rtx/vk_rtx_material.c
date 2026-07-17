/*
===========================================================================
Shared pack-time RTX material / UV-thumb helpers for world + entity albedo.
===========================================================================
*/

#include "tr_local.h"
#include "vk_rtx_material.h"
#include <math.h>

#ifdef USE_VULKAN_RTX

static const float s_rtx_mat_default[3] = { 0.72f, 0.70f, 0.66f };

void vk_rtx_material_default_rgb( float out[3] )
{
	if ( !out ) {
		return;
	}
	out[0] = s_rtx_mat_default[0];
	out[1] = s_rtx_mat_default[1];
	out[2] = s_rtx_mat_default[2];
}

void vk_rtx_material_tint_rgb( const byte *rgba, float out[3] )
{
	vk_rtx_material_default_rgb( out );
	if ( rgba && ( rgba[0] | rgba[1] | rgba[2] ) != 0 ) {
		out[0] = rgba[0] * ( 1.0f / 255.0f );
		out[1] = rgba[1] * ( 1.0f / 255.0f );
		out[2] = rgba[2] * ( 1.0f / 255.0f );
	}
}

/*
 * Prefer shader->lightingStage / lightingBundle (dlight diffuse pick), then
 * scan stages. Skips lightmap bundles — those are lighting, not albedo.
 */
static image_t *vk_rtx_material_image_from_bundle( const textureBundle_t *bundle, qboolean needThumb )
{
	image_t *img;

	if ( !bundle || bundle->lightmap != LIGHTMAP_INDEX_NONE ) {
		return NULL;
	}
	if ( bundle->rgbGen == CGEN_CONST ) {
		return NULL;
	}
	img = bundle->image[0];
	if ( !img || img == tr.defaultImage || img == tr.whiteImage ) {
		return NULL;
	}
	if ( needThumb && !img->hasThumb ) {
		R_EnsureImageThumb( img );
	}
	if ( needThumb && !img->hasThumb ) {
		return NULL;
	}
	return img;
}

static qboolean vk_rtx_material_avg_from_bundle( const textureBundle_t *bundle, float out[3] )
{
	image_t *img;

	if ( !bundle || !out || bundle->lightmap != LIGHTMAP_INDEX_NONE ) {
		return qfalse;
	}
	if ( bundle->rgbGen == CGEN_CONST ) {
		out[0] = bundle->constantColor.rgba[0] * ( 1.0f / 255.0f );
		out[1] = bundle->constantColor.rgba[1] * ( 1.0f / 255.0f );
		out[2] = bundle->constantColor.rgba[2] * ( 1.0f / 255.0f );
		return qtrue;
	}
	img = bundle->image[0];
	if ( !img || img == tr.defaultImage ) {
		return qfalse;
	}
	if ( img == tr.whiteImage ) {
		out[0] = out[1] = out[2] = 1.0f;
	} else {
		out[0] = img->avgColor[0];
		out[1] = img->avgColor[1];
		out[2] = img->avgColor[2];
	}
	if ( bundle->rgbGen == CGEN_IDENTITY_LIGHTING ) {
		out[0] *= tr.identityLight;
		out[1] *= tr.identityLight;
		out[2] *= tr.identityLight;
	}
	return qtrue;
}

static image_t *vk_rtx_material_image_from_stage( const shaderStage_t *st, int bundleIdx, qboolean needThumb )
{
	int b;

	if ( !st || !st->active ) {
		return NULL;
	}
	if ( bundleIdx < 0 || bundleIdx >= NUM_TEXTURE_BUNDLES ) {
		bundleIdx = 0;
	}
	/* Prefer requested bundle, then other non-lightmap bundles on this stage. */
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int idx = ( b == 0 ) ? bundleIdx : ( ( bundleIdx + b ) % NUM_TEXTURE_BUNDLES );
		image_t *img = vk_rtx_material_image_from_bundle( &st->bundle[idx], needThumb );
		if ( img ) {
			return img;
		}
	}
	return NULL;
}

static qboolean vk_rtx_material_avg_from_stage( const shaderStage_t *st, int bundleIdx, float out[3] )
{
	int b;

	if ( !st || !st->active || !out ) {
		return qfalse;
	}
	if ( bundleIdx < 0 || bundleIdx >= NUM_TEXTURE_BUNDLES ) {
		bundleIdx = 0;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int idx = ( b == 0 ) ? bundleIdx : ( ( bundleIdx + b ) % NUM_TEXTURE_BUNDLES );
		if ( vk_rtx_material_avg_from_bundle( &st->bundle[idx], out ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

image_t *vk_rtx_material_diffuse_image( const shader_t *shader )
{
	int i;
	image_t *img;

	if ( !shader ) {
		return NULL;
	}
	if ( shader->lightingStage >= 0 && shader->lightingStage < MAX_SHADER_STAGES ) {
		img = vk_rtx_material_image_from_stage( shader->stages[shader->lightingStage],
			shader->lightingBundle, qtrue );
		if ( img ) {
			return img;
		}
	}
	for ( i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++ ) {
		img = vk_rtx_material_image_from_stage( shader->stages[i], 0, qtrue );
		if ( img ) {
			return img;
		}
	}
	return NULL;
}

void vk_rtx_material_sample_thumb_uv( const image_t *img, float u, float v, float out[3] )
{
	int x, y;
	const byte *p;
	float fu, fv;

	if ( !out ) {
		return;
	}
	if ( !img || !img->hasThumb ) {
		vk_rtx_material_default_rgb( out );
		return;
	}
	fu = u - (float)floor( u );
	fv = v - (float)floor( v );
	if ( fu < 0.0f ) {
		fu += 1.0f;
	}
	if ( fv < 0.0f ) {
		fv += 1.0f;
	}
	x = (int)( fu * (float)( TR_IMAGE_THUMB_SIZE - 1 ) + 0.5f );
	y = (int)( fv * (float)( TR_IMAGE_THUMB_SIZE - 1 ) + 0.5f );
	if ( x < 0 ) {
		x = 0;
	} else if ( x >= TR_IMAGE_THUMB_SIZE ) {
		x = TR_IMAGE_THUMB_SIZE - 1;
	}
	if ( y < 0 ) {
		y = 0;
	} else if ( y >= TR_IMAGE_THUMB_SIZE ) {
		y = TR_IMAGE_THUMB_SIZE - 1;
	}
	p = img->thumbRGBA + ( y * TR_IMAGE_THUMB_SIZE + x ) * 4;
	out[0] = p[0] * ( 1.0f / 255.0f );
	out[1] = p[1] * ( 1.0f / 255.0f );
	out[2] = p[2] * ( 1.0f / 255.0f );
}

qboolean vk_rtx_material_avg_from_shader( const shader_t *shader, float out[3] )
{
	int i;

	if ( !shader || !out ) {
		return qfalse;
	}
	if ( shader->lightingStage >= 0 && shader->lightingStage < MAX_SHADER_STAGES ) {
		if ( vk_rtx_material_avg_from_stage( shader->stages[shader->lightingStage],
			shader->lightingBundle, out ) ) {
			return qtrue;
		}
	}
	for ( i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++ ) {
		if ( vk_rtx_material_avg_from_stage( shader->stages[i], 0, out ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

void vk_rtx_material_resolve_albedo( const shader_t *shader,
	qboolean useMaterials, qboolean useUv, float u, float v,
	const float fallback[3], const byte *tintRgba, float out[3] )
{
	image_t *img;
	float tint[3];
	qboolean haveTint;

	if ( !out || !fallback ) {
		return;
	}
	out[0] = fallback[0];
	out[1] = fallback[1];
	out[2] = fallback[2];

	if ( !useMaterials || !shader ) {
		return;
	}

	haveTint = ( tintRgba && ( tintRgba[0] | tintRgba[1] | tintRgba[2] ) != 0 ) ? qtrue : qfalse;

	if ( useUv ) {
		img = vk_rtx_material_diffuse_image( shader );
		if ( img ) {
			vk_rtx_material_sample_thumb_uv( img, u, v, out );
			if ( haveTint ) {
				vk_rtx_material_tint_rgb( tintRgba, tint );
				out[0] *= tint[0];
				out[1] *= tint[1];
				out[2] *= tint[2];
			}
			return;
		}
	}

	if ( vk_rtx_material_avg_from_shader( shader, out ) ) {
		if ( haveTint ) {
			vk_rtx_material_tint_rgb( tintRgba, tint );
			out[0] *= tint[0];
			out[1] *= tint[1];
			out[2] *= tint[2];
		}
		return;
	}

	out[0] = fallback[0];
	out[1] = fallback[1];
	out[2] = fallback[2];
}

#else /* !USE_VULKAN_RTX */

void vk_rtx_material_default_rgb( float out[3] )
{
	if ( out ) {
		out[0] = 0.72f;
		out[1] = 0.70f;
		out[2] = 0.66f;
	}
}

void vk_rtx_material_tint_rgb( const byte *rgba, float out[3] )
{
	(void)rgba;
	vk_rtx_material_default_rgb( out );
}

image_t *vk_rtx_material_diffuse_image( const shader_t *shader )
{
	(void)shader;
	return NULL;
}

void vk_rtx_material_sample_thumb_uv( const image_t *img, float u, float v, float out[3] )
{
	(void)img;
	(void)u;
	(void)v;
	vk_rtx_material_default_rgb( out );
}

qboolean vk_rtx_material_avg_from_shader( const shader_t *shader, float out[3] )
{
	(void)shader;
	(void)out;
	return qfalse;
}

void vk_rtx_material_resolve_albedo( const shader_t *shader,
	qboolean useMaterials, qboolean useUv, float u, float v,
	const float fallback[3], const byte *tintRgba, float out[3] )
{
	(void)shader;
	(void)useMaterials;
	(void)useUv;
	(void)u;
	(void)v;
	(void)tintRgba;
	if ( out && fallback ) {
		out[0] = fallback[0];
		out[1] = fallback[1];
		out[2] = fallback[2];
	}
}

#endif /* USE_VULKAN_RTX */
