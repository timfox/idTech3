#pragma once

/*
 * Renderer IQ P1 hub — reference profile, temporal history registry,
 * ghost isolation, G-buffer quality, MSAA policy, honest multi-level certification.
 * See docs/RENDERER_P1_CERTIFICATION.md and docs/RENDERER_IQ_LIVE_CERTIFICATION.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum rendererHistoryOwner_e {
	HISTORY_TAA = 0,
	HISTORY_WEAPON,
	HISTORY_SSR,
	HISTORY_AO,
	HISTORY_VOLUMETRIC,
	HISTORY_EXPOSURE,
	HISTORY_BLOOM,
	HISTORY_SHADOW,
	HISTORY_TRANSPARENCY,
	HISTORY_OTHER,
	HISTORY_OWNER_COUNT
} rendererHistoryOwner_t;

typedef enum rendererP1Gate_e {
	P1_GATE_BLOOM_SOURCE = 0,
	P1_GATE_BLOOM_FIREFLY,
	P1_GATE_NO_UNOWNED_HISTORY,
	P1_GATE_VELOCITY,
	P1_GATE_SPECULAR_STABILITY,
	P1_GATE_GBUFFER_FULL_FIDELITY,
	P1_GATE_DEFERRED_FORWARD_PARITY,
	P1_GATE_LIGHTING_OWNERSHIP,
	P1_GATE_CLUSTER_PARITY,
	P1_GATE_EDGE_REFERENCE,
	P1_GATE_SMAA,
	P1_GATE_MSAA_POLICY,
	P1_GATE_TEXTURE_LOD,
	P1_GATE_COUNT
} rendererP1Gate_t;

/*
 * Honest ladder: profile/cvar checklist alone may reach at most PROFILE_CERTIFIED.
 * IMAGE_QUALITY_CERTIFIED requires measured GPU evidence (see vk_renderer_p1_cert).
 */
typedef enum rendererP1Level_e {
	RENDERER_P1_UNCERTIFIED = 0,
	RENDERER_P1_STATIC_READY,
	RENDERER_P1_PROFILE_CERTIFIED,
	RENDERER_P1_GPU_CORE_CERTIFIED,
	RENDERER_P1_TEMPORAL_CERTIFIED,
	RENDERER_P1_EDGE_CERTIFIED,
	RENDERER_P1_LIGHTING_PARITY_CERTIFIED,
	RENDERER_P1_IMAGE_QUALITY_CERTIFIED
} rendererP1Level_t;

typedef enum rendererP1Evidence_e {
	P1_EVIDENCE_NONE = 0,
	P1_EVIDENCE_STATIC,
	P1_EVIDENCE_GPU_READBACK,
	P1_EVIDENCE_SOAK,
	P1_EVIDENCE_PENDING,
	P1_EVIDENCE_MANUAL_OVERRIDE
} rendererP1Evidence_t;

void vk_renderer_iq_p1_register( void );
void vk_renderer_iq_p1_begin_frame( void );

/* Apply modern_raster_iq_reference defaults (requires vid_restart for latched). */
void vk_renderer_iq_profile_apply( void );
qboolean vk_renderer_iq_profile_validate( char *errBuf, int errBufSize );

void vk_temporal_history_note( rendererHistoryOwner_t owner, qboolean valid,
	const char *resetReason );
void vk_temporal_history_status_f( void );
qboolean vk_temporal_history_noted_this_frame( rendererHistoryOwner_t owner );
qboolean vk_temporal_history_unowned_active( void ); /* active consumer without note this frame */

int vk_ghost_isolation_mode( void ); /* r_ghostIsolation */

int vk_gbuffer_quality_effective( void ); /* 0 compact 1 balanced 2 full */

rendererP1Level_t vk_renderer_p1_level( void );
const char *vk_renderer_p1_level_name( rendererP1Level_t lvl );
const char *vk_renderer_p1_evidence_name( rendererP1Evidence_t e );
rendererP1Evidence_t vk_renderer_p1_gate_evidence( rendererP1Gate_t gate );

void vk_renderer_p1_status_f( void );

#endif /* USE_VULKAN */
