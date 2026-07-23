#pragma once

/*
 * Production WBOIT contract freeze (Color Pipeline Phase 2.1).
 * Exact accumulation / revealage / blend / resolve / fog / HDR rules.
 * See docs/COLOR_PIPELINE.md § Phase 2.1 and docs/WBOIT_CONTRACT.md.
 *
 * Do not change fields without bumping OIT_CONTRACT_VERSION and updating docs.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define OIT_CONTRACT_VERSION 2u

typedef enum {
	OIT_ALPHA_STRAIGHT = 0,       /* material α before WBOIT premultiply */
	OIT_ALPHA_PREMULTIPLIED
} oitAlphaEncoding_t;

typedef enum {
	OIT_ACCUM_WEIGHTED_COLOR = 0  /* out = (lit*α*w, α*w); RT0 ONE/ONE */
} oitAccumulationMode_t;

typedef enum {
	OIT_REVEAL_PRODUCT_ONE_MINUS_ALPHA = 0 /* shader writes α; blend → ∏(1-α) */
} oitRevealageMode_t;

#include "vk_oit_weight_contract.h"

typedef enum {
	OIT_DEPTH_REVERSED_Z_GREATER_OR_EQUAL = 0
} oitDepthConvention_t;

typedef enum {
	OIT_RESOLVE_MCGUIRE_BAVOIL = 0
} oitResolveMode_t;

typedef struct oitContract_s {
	oitAlphaEncoding_t sourceAlphaEncoding;
	oitAccumulationMode_t accumulationMode;
	oitRevealageMode_t revealageMode;
	oitWeightMode_t weightMode;
	oitDepthConvention_t depthConvention;
	oitResolveMode_t resolveMode;

	qboolean sceneLinear;            /* SCENE_LINEAR_HDR (not display-encoded) */
	qboolean preExposed;             /* false: accum/resolve before exposure */
	qboolean premultipliedRadiance;  /* accum RGB already × α × w */
	qboolean fogAppliedPerFragment;  /* r_oitFogMode>=1: lit *= T(viewDepth) */
	qboolean emptyPixelPreservesOpaque;
	qboolean additiveSkipsRevealage; /* classify additive: reveal write-mask off */

	uint32_t accumFormat;            /* VK_FORMAT_R16G16B16A16_SFLOAT */
	uint32_t revealageFormat;        /* VK_FORMAT_R16_SFLOAT */

	float accumClear[4];             /* (0,0,0,0) */
	float revealageClear;            /* 1.0 */

	/* Vulkan blend factors for alpha (glass) bucket — stored as VkBlendFactor values. */
	uint32_t accumSrcColorBlend;     /* ONE */
	uint32_t accumDstColorBlend;     /* ONE */
	uint32_t revealSrcColorBlend;    /* ZERO */
	uint32_t revealDstColorBlend;    /* ONE_MINUS_SRC_COLOR */
	uint32_t depthCompareOp;         /* GREATER_OR_EQUAL */
	qboolean depthWrite;

	uint32_t contractVersion;
	uint32_t contractHash;
} oitContract_t;

/* Frozen production WBOIT contract (r_oit 1). Immutable logical defaults. */
const oitContract_t *vk_oit_contract_wboit( void );

/* Recompute hash; returns qtrue if matches frozen hash. */
qboolean vk_oit_contract_validate( const oitContract_t *c, char *errBuf, int errBufSize );

uint32_t vk_oit_contract_compute_hash( const oitContract_t *c );

void vk_oit_contract_print( const oitContract_t *c );
void vk_oit_contract_register( void );

const char *vk_oit_alpha_encoding_name( oitAlphaEncoding_t e );
const char *vk_oit_accum_mode_name( oitAccumulationMode_t m );
const char *vk_oit_revealage_mode_name( oitRevealageMode_t m );
const char *vk_oit_resolve_mode_name( oitResolveMode_t m );
/* Weight mode names: vk_oit_weight_mode_name in vk_oit_weight_contract.h */

#endif /* USE_VULKAN */
