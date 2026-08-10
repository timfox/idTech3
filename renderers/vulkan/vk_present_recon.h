#pragma once


/*
 * Raster Ultra 1.5 — Present-Time Adaptive Reconstruction.
 *
 * Current simulation frame is authoritative. Temporal history is optional evidence.
 * Forbidden: frame generation, interpolated presentation frames, intentional +1
 * frame presentation latency, weapon history poisoning.
 */

extern cvar_t *r_presentAdaptiveBudget;
extern cvar_t *r_presentAdaptiveSpatial;
extern cvar_t *r_presentAdaptiveHistoryCap;
extern cvar_t *r_debugAdaptiveSampleMask;

void vk_present_recon_register_cvars( void );
void vk_present_recon_init( void );
void vk_present_recon_shutdown( void );

/* True when r_aaMode 3 or (r_presentAdaptiveRecon && temporal reconstruction). */
qboolean vk_present_recon_active( void );

/* Strict current-frame-first policy (lower history caps, spatial fallback). */
qboolean vk_present_recon_wants_adaptive( void );

void vk_present_recon_begin_frame( void );
void vk_present_recon_note_gpu_submit( void );
void vk_present_recon_note_present( void );
void vk_present_recon_status_f( void );
void vk_motion_vector_cert_status_f( void );

