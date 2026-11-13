#pragma once

/*
 * Phase 1.5 — renderer-owned deterministic IQ certification fixtures.
 * ROIs describe expected analysis regions; optional overlay draws when armed.
 */


#include "../common/tr_types.h"

#define IQ_CERT_MAX_ROIS 16

typedef enum {
	IQ_CERT_FIXTURE_NONE = 0,
	IQ_CERT_FIXTURE_FIREFLY,
	IQ_CERT_FIXTURE_EDGE_VERT,
	IQ_CERT_FIXTURE_EDGE_DIAG,
	IQ_CERT_FIXTURE_ROUGHNESS_LADDER,
	IQ_CERT_FIXTURE_MOTION_STRIPE,
	IQ_CERT_FIXTURE_GBUFFER_RAMPS
} iqCertFixture_t;

typedef struct iqCertRoi_s {
	float u0, v0, u1, v1; /* normalized 0..1 */
	float expectLuma;
	float expectEdgeWidthPx;
	float expectVelocityMag;
	char label[32];
} iqCertRoi_t;

typedef struct iqCertScenario_s {
	char name[64];
	uint32_t seed;
	iqCertFixture_t fixture;
	uint32_t roiCount;
	iqCertRoi_t rois[IQ_CERT_MAX_ROIS];
	float fireflySpikeLuma;
	float fireflySheetLuma;
	float edgeContrast;
	qboolean armedThisFrame;
} iqCertScenario_t;

void vk_iq_cert_geometry_register( void );

void vk_iq_cert_geometry_clear( void );
void vk_iq_cert_geometry_arm( const iqCertScenario_t *scenario );
qboolean vk_iq_cert_geometry_armed( void );
const iqCertScenario_t *vk_iq_cert_geometry_scenario( void );

void vk_iq_cert_geometry_make_firefly( iqCertScenario_t *out );
void vk_iq_cert_geometry_make_edge_vert( iqCertScenario_t *out );
void vk_iq_cert_geometry_make_edge_diag( iqCertScenario_t *out );
void vk_iq_cert_geometry_make_roughness_ladder( iqCertScenario_t *out );
void vk_iq_cert_geometry_make_motion_stripe( iqCertScenario_t *out );
void vk_iq_cert_geometry_make_gbuffer_ramps( iqCertScenario_t *out );

