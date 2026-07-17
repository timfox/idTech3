#pragma once

#include "tr_local.h"

/*
 * Shared pack-time RTX material helpers (world + entity albedo SSBOs).
 * Prefers shader lightingStage/lightingBundle (dlight diffuse), then first
 * usable non-lightmap stage. UV path uses 8×8 image thumbs.
 */

void vk_rtx_material_default_rgb( float out[3] );
void vk_rtx_material_tint_rgb( const byte *rgba, float out[3] );

/* Prefer lightingStage diffuse image with hasThumb, else first usable stage. */
image_t *vk_rtx_material_diffuse_image( const shader_t *shader );

/* Sample thumb at wrapped UV; writes default gray if no thumb. */
void vk_rtx_material_sample_thumb_uv( const image_t *img, float u, float v, float out[3] );

/*
 * Diffuse avgColor / const RGB from lightingStage (preferred) or first usable stage.
 * Returns qfalse when no usable material color was found.
 */
qboolean vk_rtx_material_avg_from_shader( const shader_t *shader, float out[3] );

/*
 * Resolve pack-time prim albedo:
 *  !useMaterials → fallback
 *  useUv + thumb → UV sample (then * tint if tint nonzero)
 *  else shader avg → that (then * tint if tint nonzero)
 *  else → fallback
 * tintRgba may be NULL (world). fallback must be non-NULL.
 */
void vk_rtx_material_resolve_albedo( const shader_t *shader,
	qboolean useMaterials, qboolean useUv, float u, float v,
	const float fallback[3], const byte *tintRgba, float out[3] );
