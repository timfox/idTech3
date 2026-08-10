#pragma once
void vk_iq_lab_register( void );
/* After bloom extract (same command buffer) — record IQ GPU snapshot when lab armed. */
void vk_iq_lab_on_bloom_extract( void );
/* Optional: after G-buffer fill when lab armed. */
void vk_iq_lab_on_gbuffer_ready( void );
/* After rendering_finished_fence for cmdIndex — finalize snapshot and evaluate. */
void vk_iq_lab_finalize_frame( int cmdIndex );
/* True while cert fixtures should isolate world draws. */
qboolean vk_iq_lab_isolate_world( void );
qboolean vk_iq_lab_armed( void );
