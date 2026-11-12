#pragma once

/*
 * Color Pipeline Reconstruction — Phase 1 authoritative color contract.
 *
 * All opaque, transparent, weapon, volumetric, bloom, exposure, and display
 * work must operate under one explicit space + stage order.
 * See docs/COLOR_PIPELINE.md.
 */


#include "../common/tr_types.h"

typedef enum {
	VK_COLOR_SPACE_TEXTURE_SRGB = 0,
	VK_COLOR_SPACE_TEXTURE_LINEAR,
	VK_COLOR_SPACE_SCENE_LINEAR_HDR,
	VK_COLOR_SPACE_PREEXPOSED_SCENE_LINEAR_HDR,
	VK_COLOR_SPACE_DISPLAY_LINEAR,
	VK_COLOR_SPACE_DISPLAY_ENCODED,
	VK_COLOR_SPACE_COUNT
} vkColorSpace_t;

typedef enum {
	VK_ALPHA_ENCODING_OPAQUE = 0,
	VK_ALPHA_ENCODING_STRAIGHT,
	VK_ALPHA_ENCODING_PREMULTIPLIED,
	VK_ALPHA_ENCODING_COVERAGE, /* WBOIT reveal / product(1-a) */
	VK_ALPHA_ENCODING_COUNT
} vkAlphaEncoding_t;

/*
 * Required production order (docs/COLOR_PIPELINE.md).
 * Do not reorder without updating docs + validate.
 */
typedef enum {
	VK_COLOR_STAGE_TEXTURE_DECODE = 0,
	VK_COLOR_STAGE_MATERIAL_EVAL,
	VK_COLOR_STAGE_OPAQUE_LIGHTING,
	VK_COLOR_STAGE_SKY_ATMOSPHERE,
	VK_COLOR_STAGE_OPAQUE_HDR_COMPOSITE,
	VK_COLOR_STAGE_TRANSPARENT_LIGHTING,
	VK_COLOR_STAGE_OIT_ACCUM,
	VK_COLOR_STAGE_OIT_RESOLVE,
	VK_COLOR_STAGE_REFRACTION,
	VK_COLOR_STAGE_WEAPON_HDR,
	VK_COLOR_STAGE_VOLUMETRIC,
	VK_COLOR_STAGE_BLOOM,
	VK_COLOR_STAGE_EXPOSURE,
	VK_COLOR_STAGE_TONEMAP,
	VK_COLOR_STAGE_COLOR_GRADE,
	VK_COLOR_STAGE_DISPLAY_TRANSFORM,
	VK_COLOR_STAGE_UI,
	VK_COLOR_STAGE_COUNT
} vkColorPipelineStage_t;

void vk_color_contract_register( void );
void vk_color_contract_begin_frame( void );

/* Annotate the last writer for a pipeline stage (CPU bookkeeping). */
void vk_color_contract_note_stage( vkColorPipelineStage_t stage, const char *passName,
	vkColorSpace_t space, vkAlphaEncoding_t alpha );

const char *vk_color_space_name( vkColorSpace_t space );
const char *vk_color_stage_name( vkColorPipelineStage_t stage );
const char *vk_alpha_encoding_name( vkAlphaEncoding_t alpha );

/* Expected space / alpha for each stage (contract table). */
vkColorSpace_t vk_color_stage_expected_space( vkColorPipelineStage_t stage );
vkAlphaEncoding_t vk_color_stage_expected_alpha( vkColorPipelineStage_t stage );

/* True when r_oit 1 is the production transparency path (MBOIT remains experimental). */
qboolean vk_color_contract_wboit_is_production( void );

/* Static + runtime checks: order, spaces, OIT production policy. */
qboolean vk_color_contract_validate( char *errBuf, int errBufSize );

