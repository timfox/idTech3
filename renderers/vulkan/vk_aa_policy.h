#pragma once

#ifdef USE_VULKAN

/*
 * Layered AA policy (r_aaMode):
 *  0 none
 *  1 FXAA
 *  2 SMAA 1x (shipping default / certified zero-history)
 *  3 Present-Time Adaptive Reconstruction (Raster Ultra 1.5; migrated from SMAA T2x)
 *  4 Temporal Reconstruction (native; temporal upscale when render scale < 1)
 *  5 Temporal Reconstruction + SMAA cleanup
 *  6 Spatial supersampled reference (r_ext_supersample)
 *
 * Conceptual "mode 5 = SS reference" in Ultra 1.5 docs maps to r_aaMode 6 so mode 5
 * cleanup configs are not silently broken.
 */

void vk_aa_policy_register_cvars( void );
void vk_aa_policy_apply( void );
qboolean vk_aa_policy_wants_temporal_cleanup_smaa( void );
/* True when r_aaMode 3 (Present-Time Adaptive Reconstruction). Legacy name kept for ABI. */
qboolean vk_aa_policy_wants_smaa_t2x( void );
qboolean vk_aa_policy_wants_present_adaptive( void );

#endif /* USE_VULKAN */
