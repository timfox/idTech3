#pragma once

#ifdef USE_VULKAN

/*
 * Layered AA policy (r_aaMode):
 *  0 none
 *  1 FXAA
 *  2 SMAA 1x (shipping default)
 *  3 SMAA T2x scaffold (SMAA + light temporal until full T2x)
 *  4 Temporal Reconstruction
 *  5 Temporal Reconstruction + SMAA cleanup
 *  6 supersampled reference (r_ext_supersample)
 */

void vk_aa_policy_register_cvars( void );
void vk_aa_policy_apply( void );
qboolean vk_aa_policy_wants_temporal_cleanup_smaa( void );
qboolean vk_aa_policy_wants_smaa_t2x( void );

#endif /* USE_VULKAN */
