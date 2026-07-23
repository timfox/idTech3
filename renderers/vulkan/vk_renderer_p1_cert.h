#pragma once

/*
 * Renderer IQ Phase 1.5 — evidence-backed live P1 certification.
 * MANUAL_OVERRIDE never grants RENDERER_P1_IMAGE_QUALITY_CERTIFIED.
 * See docs/RENDERER_IQ_LIVE_CERTIFICATION.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"
#include "vk_renderer_iq_p1.h"

typedef enum {
	P1_CERT_STATUS_PENDING = 0,
	P1_CERT_STATUS_PASS,
	P1_CERT_STATUS_FAIL,
	P1_CERT_STATUS_SKIP,
	P1_CERT_STATUS_INVALIDATED
} p1CertStatus_t;

typedef enum {
	P1_CERT_STAGE_STATIC = 0,
	P1_CERT_STAGE_PROFILE,
	P1_CERT_STAGE_BLOOM_SOURCE,
	P1_CERT_STAGE_BLOOM_FIREFLY,
	P1_CERT_STAGE_BLOOM_PYRAMID,
	P1_CERT_STAGE_GBUFFER_QUANT,
	P1_CERT_STAGE_MATERIAL_DECODE,
	P1_CERT_STAGE_TEMPORAL_HISTORY,
	P1_CERT_STAGE_VELOCITY,
	P1_CERT_STAGE_TEMPORAL_RESET,
	P1_CERT_STAGE_GHOSTING,
	P1_CERT_STAGE_SPECULAR_STABILITY,
	P1_CERT_STAGE_NORMAL_MIP,
	P1_CERT_STAGE_EDGE,
	P1_CERT_STAGE_SMAA,
	P1_CERT_STAGE_MSAA_POLICY,
	P1_CERT_STAGE_TEXTURE_LOD,
	P1_CERT_STAGE_LIGHTING_PARITY,
	P1_CERT_STAGE_LIGHTING_OWNERSHIP,
	P1_CERT_STAGE_CLUSTER_PARITY,
	P1_CERT_STAGE_LIFECYCLE,
	P1_CERT_STAGE_SOAK,
	P1_CERT_STAGE_COUNT
} p1CertStage_t;

typedef struct p1CertStageResult_s {
	uint32_t stage;
	uint32_t status;
	uint32_t evidenceType;

	double observed;
	double warningThreshold;
	double failureThreshold;

	uint64_t frameNumber;
	uint64_t timestamp;

	char testName[64];
	char failureReason[256];
	char capturePrefix[256];
	char deviceNote[64];
} p1CertStageResult_t;

void vk_renderer_p1_cert_register( void );
void vk_renderer_p1_cert_begin_frame( void );

rendererP1Level_t vk_renderer_p1_cert_level( void );
const char *vk_renderer_p1_cert_stage_name( p1CertStage_t stage );
const char *vk_renderer_p1_cert_status_name( p1CertStatus_t s );

rendererP1Evidence_t vk_renderer_p1_cert_required_evidence( p1CertStage_t stage );
qboolean vk_renderer_p1_cert_evidence_satisfies( rendererP1Evidence_t have, rendererP1Evidence_t need );

const p1CertStageResult_t *vk_renderer_p1_cert_stage_result( p1CertStage_t stage );

void vk_renderer_p1_cert_record_result( const p1CertStageResult_t *result );

/* Legacy helpers — always stamp MANUAL_OVERRIDE (cannot grant final IMAGE_QUALITY). */
void vk_renderer_p1_cert_stage_pass( p1CertStage_t stage, float observed, float threshold, const char *notes );
void vk_renderer_p1_cert_stage_fail( p1CertStage_t stage, float observed, float threshold, const char *notes );
void vk_renderer_p1_cert_stage_skip( p1CertStage_t stage, const char *reason );

void vk_renderer_p1_cert_invalidate_all( const char *reason );
qboolean vk_renderer_p1_cert_export_json( const char *path );

/* Refresh STATIC + PROFILE stages from live cvars/contracts (no GPU). */
void vk_renderer_p1_cert_refresh_static( void );

#endif /* USE_VULKAN */
