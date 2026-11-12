#pragma once

/*
 * Color Pipeline Phase 2.5.1 — bounded WBOIT weight-function contract.
 * Authoritative coefficients for production accum weighting.
 * See docs/WBOIT_WEIGHT_CONTRACT.md.
 *
 * Do not change fields without bumping OIT_WEIGHT_CONTRACT_VERSION and docs.
 */


#include "../common/tr_types.h"

#define OIT_WEIGHT_CONTRACT_VERSION 1u

typedef enum oitWeightMode_e {
	OIT_WEIGHT_ALPHA_REFERENCE = 0,   /* w ≈ f(alpha) only — laboratory */
	OIT_WEIGHT_LEGACY_DEPTH,          /* pre-2.5 McGuire form (diagnostic) */
	OIT_WEIGHT_BOUNDED_PRODUCTION,    /* certified production (default) */
	OIT_WEIGHT_MATERIAL_RESEARCH      /* reserved — not Spine production */
} oitWeightMode_t;

typedef struct oitWeightContract_s {
	oitWeightMode_t mode;

	float minWeight;
	float maxWeight;
	float alphaExponent;
	float depthExponent;

	float depthScale;
	float minimumOpacityContribution;
	float nearClamp;
	float farClamp;

	uint32_t usesPositiveViewDepth;
	uint32_t contractVersion;
	uint32_t contractHash;
} oitWeightContract_t;

/* Frozen production weight contract (BOUNDED_PRODUCTION). */
const oitWeightContract_t *vk_oit_weight_contract_get( void );

uint32_t vk_oit_weight_contract_compute_hash( const oitWeightContract_t *c );
qboolean vk_oit_weight_contract_validate( const oitWeightContract_t *c, char *errBuf, int errBufSize );

void vk_oit_weight_contract_print( const oitWeightContract_t *c );
void vk_oit_weight_contract_register( void );

/* CPU mirror of oit_weight.glsl (BOUNDED_PRODUCTION / ALPHA_REFERENCE). */
float vk_oit_weight_evaluate( const oitWeightContract_t *c, float opacity, float positiveViewDepth );

const char *vk_oit_weight_mode_name( oitWeightMode_t m );

