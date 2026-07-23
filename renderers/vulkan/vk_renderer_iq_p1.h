#pragma once

/*
 * Renderer IQ P1 hub — reference profile, temporal history registry,
 * ghost isolation, G-buffer quality, MSAA policy, P1 certification.
 * See docs/RENDERER_P1_CERTIFICATION.md.
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

void vk_renderer_iq_p1_register( void );
void vk_renderer_iq_p1_begin_frame( void );

/* Apply modern_raster_iq_reference defaults (requires vid_restart for latched). */
void vk_renderer_iq_profile_apply( void );
qboolean vk_renderer_iq_profile_validate( char *errBuf, int errBufSize );

void vk_temporal_history_note( rendererHistoryOwner_t owner, qboolean valid,
	const char *resetReason );
void vk_temporal_history_status_f( void );

int vk_ghost_isolation_mode( void ); /* r_ghostIsolation */

int vk_gbuffer_quality_effective( void ); /* 0 compact 1 balanced 2 full */

void vk_renderer_p1_status_f( void );

#endif /* USE_VULKAN */
