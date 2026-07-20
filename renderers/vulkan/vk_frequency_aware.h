#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.12 — Frequency-Aware Rendering + Moiré Suppression.
 * Classify undersampling sources; apply the smallest correct filter.
 * Does NOT rely on TAA, global blur, or RT.
 */

typedef enum {
	VK_FREQ_SRC_NONE = 0,
	VK_FREQ_SRC_TEXTURE,
	VK_FREQ_SRC_NORMAL,
	VK_FREQ_SRC_SPECULAR,
	VK_FREQ_SRC_ALPHA,
	VK_FREQ_SRC_PROCEDURAL,
	VK_FREQ_SRC_GEOMETRY,
	VK_FREQ_SRC_SHADOW,
	VK_FREQ_SRC_TRANSPARENCY,
	VK_FREQ_SRC_RECONSTRUCTION,
	VK_FREQ_SRC_COUNT
} vkFreqAliasSource_t;

typedef enum {
	VK_FREQ_TIER_OFF = 0,
	VK_FREQ_TIER_LOW,
	VK_FREQ_TIER_MEDIUM,
	VK_FREQ_TIER_HIGH,
	VK_FREQ_TIER_ULTRA,
	VK_FREQ_TIER_REFERENCE
} vkFreqTier_t;

typedef enum {
	VK_FREQ_RESP_ORDINARY = 0,
	VK_FREQ_RESP_ANISO,
	VK_FREQ_RESP_COARSER_MIP,
	VK_FREQ_RESP_ALPHA_COVERAGE,
	VK_FREQ_RESP_PROC_CUTOFF,
	VK_FREQ_RESP_SPEC_VARIANCE,
	VK_FREQ_RESP_MATERIAL_LOD,
	VK_FREQ_RESP_GEOM_LOD,
	VK_FREQ_RESP_SELECTIVE_SS,
	VK_FREQ_RESP_STOCHASTIC
} vkFreqResponse_t;

typedef struct vkFreqState_s {
	vkFreqTier_t tier;
	qboolean anisotropicPolicy;
	qboolean specularNdFilter;
	qboolean alphaCoverage;
	qboolean proceduralCutoff;
	qboolean waterFrequency;
	qboolean shadowDecorrelate;
	qboolean selectiveSS;      /* experimental; default off */
	qboolean stochasticFilter;  /* experimental; default off */
	float specularAaStrength;
	float mipLodBiasClamp;     /* do not go more negative than this when active */
	uint32_t samplerAnisotropyCap;
	uint32_t frameCount;
} vkFreqState_t;

void vk_frequency_aware_register_cvars( void );
void vk_frequency_aware_init( void );
void vk_frequency_aware_shutdown( void );

qboolean vk_frequency_aware_active( void );
const vkFreqState_t *vk_frequency_aware_state( void );

/* Effective specular-AA strength (0 = leave existing path alone). */
float vk_frequency_aware_specular_aa_strength( void );

/* Coverage-preserving alpha (push reserved[4] when atest + not stochastic). */
qboolean vk_frequency_aware_alpha_coverage( void );

/* Soft clamp for aggressive negative mip bias while active. */
float vk_frequency_aware_mip_bias_floor( void );

void vk_frequency_aware_begin_frame( void );

void vk_frequency_aware_status_f( void );
void vk_frequency_aware_sampler_status_f( void );
void vk_frequency_aware_scenes_f( void );

const char *vk_frequency_aware_source_name( vkFreqAliasSource_t s );
const char *vk_frequency_aware_tier_name( vkFreqTier_t t );

#endif /* USE_VULKAN */
