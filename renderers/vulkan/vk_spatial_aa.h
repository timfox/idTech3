/*
===========================================================================
Raster Ultra 2.1 — Unified Spatial Antialiasing Controller (Slice A).
History-free / RT-free. Classifies surfaces and routes to SMAA, adaptive
current-frame supersampling, frequency-aware filters, and optional MSAA.
===========================================================================
*/

#pragma once


typedef enum {
	VK_SPATIAL_AA_TIER_OFF = 0,
	VK_SPATIAL_AA_TIER_LOW,
	VK_SPATIAL_AA_TIER_MEDIUM,
	VK_SPATIAL_AA_TIER_HIGH,
	VK_SPATIAL_AA_TIER_ULTRA,
	VK_SPATIAL_AA_TIER_REFERENCE
} vkSpatialAaTier_t;

typedef struct vkSpatialAaState_s {
	vkSpatialAaTier_t tier;
	qboolean classify;
	qboolean adaptiveSS;
	qboolean selectiveMsaa;   /* policy flag; full selective path is Slice A scaffold */
	qboolean smaaCleanup;
	qboolean frequencyAware;
	qboolean forceTaaOff;
	qboolean forceRtOff;
	float riskThreshold;
	float sampleBudget;       /* 0..1 fraction of pixels allowed to take extra taps */
	float lastEstimatedCoverage;
	uint32_t frameCount;
	uint32_t adaptivePasses;
} vkSpatialAaState_t;

void vk_spatial_aa_register_cvars( void );
void vk_spatial_aa_init( void );
void vk_spatial_aa_shutdown( void );

qboolean vk_spatial_aa_active( void );
const vkSpatialAaState_t *vk_spatial_aa_state( void );

/* Current-frame adaptive supersample pass (pre-SMAA). */
qboolean vk_spatial_aa_wants_adaptive_ss( void );
qboolean vk_spatial_aa_adaptive_pipeline_ready( void );

void vk_spatial_aa_begin_frame( void );
void vk_spatial_aa_enforce_contract( void );

/* Returns color source for SMAA after optional adaptive pass (may be history[0]). */
VkImageView vk_spatial_aa_prepare_input( VkImageView color_source );

void vk_spatial_aa_status_f( void );

const char *vk_spatial_aa_tier_name( vkSpatialAaTier_t t );

