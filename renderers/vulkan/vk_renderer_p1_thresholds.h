#pragma once

/*
 * Phase 1.6 — versioned numerical thresholds for P1 live gates.
 * Changing thresholds invalidates affected evidence (thresholdHash).
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef struct rendererP1Thresholds_s {
	uint32_t contractHash;

	/* Bloom */
	float fireflySpikeAttenuationMin;
	float fireflyCoherentRetentionMin;
	float fireflyThinLineRetentionMin;
	float fireflyFalsePositiveMax;
	float bloomCentroidShiftMaxPx;
	float bloomRadiusAsymmetryMax;
	float bloomEnergyGrowthMax;

	/* Velocity */
	float velocityMeanErrorMax;
	float velocityMaxErrorMax;
	float velocityWrongSignFracMax;

	/* Temporal / ghosting */
	float ghostTrailLengthMaxPx;
	float ghostRecoveryFramesMax;
	float disocclusionContaminationMax;

	/* Specular */
	float specularVarianceMax;
	float specularSpikeCountMax;
	float specularCloseupSharpnessMin;

	/* G-buffer */
	float gbufferNormalAngularErrorMaxDeg;
	float gbufferRoughnessAbsErrorMax;
	float gbufferIdCollisionMax;

	/* Lighting */
	float lightingMeanRgbErrorMax;
	float lightingMaxRgbErrorMax;
	float lightingOwnershipSeamMax;
	float lightingLightmapErrorMax;

	/* Clusters */
	uint32_t clusterMismatchMax;
	uint32_t clusterOverflowFailMax;

	/* Edges / SMAA */
	float edgeSpreadWidthMaxPx;
	float edgeContrastRetentionMin;
	float edgeHaloAmplitudeMax;
	float smaaMissedEdgeFracMax;

	/* Texture LOD */
	float textureTemporalVarianceMax;
	float textureMoireEnergyMax;
} rendererP1Thresholds_t;

void vk_renderer_p1_thresholds_register( void );
const rendererP1Thresholds_t *vk_renderer_p1_thresholds_get( void );
uint32_t vk_renderer_p1_thresholds_hash( void );
qboolean vk_renderer_p1_thresholds_validate( char *errBuf, int errBufSize );
void vk_renderer_p1_thresholds_export_json( const char *path );

#endif /* USE_VULKAN */
