#pragma once

void vk_volumetric_skip_cleanup( const char *reason, uint32_t restoreDepthSrcStages );
void vk_volumetric_fog_pass( void );
/* When r_oitFogMode>=1, fog opaque HDR before WBOIT so resolve composites over fogged
 * background and frame-end volumetric is skipped (avoids double-haze on glass). */
void vk_volumetric_fog_before_oit( void );
