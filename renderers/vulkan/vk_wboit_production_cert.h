#pragma once

/*
 * Color Pipeline Phase 2.6A — evidence-backed live WBOIT certification.
 * Manual stage flags NEVER grant WBOIT_PRODUCTION_CERTIFIED unless
 * r_oitAllowManualCertification is explicitly enabled (still not default production).
 * See docs/WBOIT_LIVE_CERTIFICATION.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum wboitCertEvidence_e {
	WBOIT_EVIDENCE_NONE = 0,
	WBOIT_EVIDENCE_STATIC,
	WBOIT_EVIDENCE_CPU_REFERENCE,
	WBOIT_EVIDENCE_GPU_READBACK,
	WBOIT_EVIDENCE_GPU_IMAGE_DIFF,
	WBOIT_EVIDENCE_GPU_REDUCTION,
	WBOIT_EVIDENCE_LIFECYCLE,
	WBOIT_EVIDENCE_SOAK,
	WBOIT_EVIDENCE_MANUAL_OVERRIDE
} wboitCertEvidence_t;

typedef enum {
	WBOIT_CERT_STATUS_PENDING = 0,
	WBOIT_CERT_STATUS_PASS,
	WBOIT_CERT_STATUS_FAIL,
	WBOIT_CERT_STATUS_SKIP,
	WBOIT_CERT_STATUS_INVALIDATED
} wboitCertStatus_t;

typedef enum {
	WBOIT_CERT_STAGE_CONTRACT = 0,
	WBOIT_CERT_STAGE_RESOURCES,
	WBOIT_CERT_STAGE_EMPTY_PIXEL,
	WBOIT_CERT_STAGE_SINGLE_LAYER,
	WBOIT_CERT_STAGE_REVEALAGE,
	WBOIT_CERT_STAGE_ALPHA_ENCODING,
	WBOIT_CERT_STAGE_WEIGHT_BOUNDS,
	WBOIT_CERT_STAGE_ORDER_STABILITY,
	WBOIT_CERT_STAGE_FOG_DEPTH,
	WBOIT_CERT_STAGE_ADDITIVE,
	WBOIT_CERT_STAGE_HDR_RESOLVE,
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

typedef struct wboitCertStageResult_s {
	uint32_t stage;
	uint32_t status;
	uint32_t evidenceType;

	double observed;
	double warningThreshold;
	double failureThreshold;

	uint64_t frameNumber;
	uint64_t timestamp;

	uint32_t oitContractHash;
	uint32_t alphaContractHash;
	uint32_t depthContractHash;
	uint32_t weightContractHash;
	uint32_t resolveContractHash;

	uint32_t sceneGeneration;
	uint32_t depthGeneration;
	uint32_t oitGeneration;
	uint32_t resolveGeneration;

	char testName[64];
	char failureReason[256];
	char capturePrefix[256];
} wboitCertStageResult_t;

void vk_wboit_production_cert_register( void );
void vk_wboit_production_cert_begin_frame( void );

wboitProductionLevel_t vk_wboit_production_level( void );
const char *vk_wboit_production_level_name( wboitProductionLevel_t lvl );
const char *vk_wboit_cert_stage_name( wboitCertStage_t stage );
const char *vk_wboit_cert_evidence_name( wboitCertEvidence_t e );
const char *vk_wboit_cert_status_name( wboitCertStatus_t s );

wboitCertEvidence_t vk_wboit_cert_required_evidence( wboitCertStage_t stage );
qboolean vk_wboit_cert_evidence_satisfies( wboitCertEvidence_t have, wboitCertEvidence_t need );

const wboitCertStageResult_t *vk_wboit_cert_stage_result( wboitCertStage_t stage );

/* Evidence-bearing result API (preferred). */
void vk_wboit_cert_record_result( const wboitCertStageResult_t *result );

/* Legacy helpers — always stamp MANUAL_OVERRIDE (cannot promote to PRODUCTION by default). */
void vk_wboit_cert_stage_pass( wboitCertStage_t stage, float observed, float threshold, const char *notes );
void vk_wboit_cert_stage_fail( wboitCertStage_t stage, float observed, float threshold,
	const char *material, const char *region, const char *notes );
void vk_wboit_cert_stage_skip( wboitCertStage_t stage, const char *reason );

void vk_wboit_cert_invalidate_all( const char *reason );
qboolean vk_wboit_cert_export_json( const char *path );
qboolean vk_wboit_cert_import_json( const char *path ); /* display only — never certifies this device */

/* CPU reference helpers. */
float vk_wboit_cert_revealage_product( const float *alphas, int count );
void vk_wboit_cert_source_over( const float layerRgb[3], float opacity,
	const float fogRgb[3], float outRgb[3] );

#endif /* USE_VULKAN */
