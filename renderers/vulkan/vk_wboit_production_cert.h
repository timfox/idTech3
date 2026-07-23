#pragma once

/*
 * Color Pipeline Phase 2.6 — live WBOIT production certification stages/levels.
 * Complements legacy B0–B7 operator matrix in vk_oit_certify.c.
 * Static gates alone NEVER grant WBOIT_PRODUCTION_CERTIFIED.
 * See docs/WBOIT_LIVE_CERTIFICATION.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum {
	WBOIT_CERT_STAGE_CONTRACT = 0,
	WBOIT_CERT_STAGE_RESOURCES,
	WBOIT_CERT_STAGE_EMPTY_PIXEL,
	WBOIT_CERT_STAGE_SINGLE_LAYER,
	WBOIT_CERT_STAGE_REVEALAGE,
	WBOIT_CERT_STAGE_ORDER_STABILITY,
	WBOIT_CERT_STAGE_ALPHA_ENCODING,
	WBOIT_CERT_STAGE_DEPTH,
	WBOIT_CERT_STAGE_FOG,
	WBOIT_CERT_STAGE_ADDITIVE,
	WBOIT_CERT_STAGE_HDR_RESOLVE,
	WBOIT_CERT_STAGE_EXPOSURE,
	WBOIT_CERT_STAGE_LIFECYCLE,
	WBOIT_CERT_STAGE_SOAK,
	WBOIT_CERT_STAGE_COUNT
} wboitCertStage_t;

typedef enum {
	WBOIT_LEVEL_NONE = 0,
	WBOIT_STATIC_CERTIFIED,
	WBOIT_GPU_CORE_CERTIFIED,
	WBOIT_FOG_HDR_CERTIFIED,
	WBOIT_LIFECYCLE_CERTIFIED,
	WBOIT_PRODUCTION_CERTIFIED
} wboitProductionLevel_t;

typedef struct {
	char result[16]; /* PASS/FAIL/PENDING/SKIP */
	float observed;
	float threshold;
	char failingMaterial[64];
	char failingRegion[64];
	char capturePath[128];
	uint32_t oitContractHash;
	uint32_t weightContractHash;
	uint32_t hdrResolveHash;
	uint32_t fogSceneGen;
	uint32_t oitAttGen;
	char notes[192];
} wboitCertStageReport_t;

void vk_wboit_production_cert_register( void );
void vk_wboit_production_cert_begin_frame( void );

wboitProductionLevel_t vk_wboit_production_level( void );
const char *vk_wboit_production_level_name( wboitProductionLevel_t lvl );
const char *vk_wboit_cert_stage_name( wboitCertStage_t stage );

/* Operator / lab hooks — mark stage outcomes (GPU metrics filled when available). */
void vk_wboit_cert_stage_pass( wboitCertStage_t stage, float observed, float threshold, const char *notes );
void vk_wboit_cert_stage_fail( wboitCertStage_t stage, float observed, float threshold,
	const char *material, const char *region, const char *notes );
void vk_wboit_cert_stage_skip( wboitCertStage_t stage, const char *reason );

/* CPU reference helpers used by lab / unit tests. */
float vk_wboit_cert_revealage_product( const float *alphas, int count );
void vk_wboit_cert_source_over( const float layerRgb[3], float opacity,
	const float fogRgb[3], float outRgb[3] );

#endif /* USE_VULKAN */
