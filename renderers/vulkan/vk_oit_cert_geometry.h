#pragma once

/*
 * Phase 2.6B — renderer-owned deterministic WBOIT certification geometry.
 * Fixtures draw through the live oit_accum / additive pipelines during vk_oit_pass.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define OIT_CERT_MAX_PANES 32

typedef enum {
	OIT_CERT_BLEND_ALPHA = 0,   /* ordinary WBOIT (SRC_ALPHA / ONE_MINUS) */
	OIT_CERT_BLEND_ADDITIVE     /* additive bucket (ONE / ONE) */
} oitCertBlend_t;

typedef struct oitCertPane_s {
	float color[3];       /* unassociated radiance (SCENE_LINEAR_HDR) */
	float opacity;        /* straight alpha 0..1 */
	float viewDepth;      /* positive view-depth meters along axis[0] */
	float halfWidth;      /* world units */
	float halfHeight;
	oitCertBlend_t blend;
	qboolean fogged;      /* subject to r_oitFogMode density */
	uint32_t layerId;     /* permutation identity */
} oitCertPane_t;

typedef struct oitCertScenario_s {
	char name[64];
	uint32_t seed;
	uint32_t paneCount;
	oitCertPane_t panes[OIT_CERT_MAX_PANES];
	/* Expected CPU reference (filled by geometry helper). */
	float expectRevealage;          /* product(1-a) for alpha panes only */
	float expectSingleOpacity;
	float expectSingleColor[3];
	qboolean expectEmpty;           /* no alpha panes */
	qboolean drawnThisFrame;
} oitCertScenario_t;

void vk_oit_cert_geometry_register( void );

/* Arm a scenario for the next OIT accum (clears previous). */
void vk_oit_cert_geometry_clear( void );
void vk_oit_cert_geometry_arm( const oitCertScenario_t *scenario );
qboolean vk_oit_cert_geometry_armed( void );
const oitCertScenario_t *vk_oit_cert_geometry_scenario( void );

/* Built-in scenarios. */
void vk_oit_cert_geometry_make_empty( oitCertScenario_t *out );
void vk_oit_cert_geometry_make_single_layer( oitCertScenario_t *out, float opacity,
	const float color[3], float viewDepth );
void vk_oit_cert_geometry_make_revealage_layers( oitCertScenario_t *out, const float *alphas, int count,
	float viewDepth );
void vk_oit_cert_geometry_make_weight_ladder( oitCertScenario_t *out );
void vk_oit_cert_geometry_make_order_rgb( oitCertScenario_t *out, int permutation /* 0..5 */ );
void vk_oit_cert_geometry_make_fog_depth_ladder( oitCertScenario_t *out );
void vk_oit_cert_geometry_make_additive_over_glass( oitCertScenario_t *out );

/* Called from vk_oit_pass while oitAccumPass is active (per bucket). */
void vk_oit_cert_geometry_draw_bucket( int bucketFilter /* 0=all 1=alpha 2=additive */ );

/* After resolve: true if fixtures were drawn this frame. */
qboolean vk_oit_cert_geometry_was_drawn( void );

/* CPU expected resolve for single-layer over fog RGB. */
void vk_oit_cert_geometry_expect_source_over( const float layerRgb[3], float opacity,
	const float fogRgb[3], float outRgb[3] );

#endif /* USE_VULKAN */
