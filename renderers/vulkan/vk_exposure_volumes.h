/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 */
#ifndef VK_EXPOSURE_VOLUMES_H
#define VK_EXPOSURE_VOLUMES_H

void vk_exposure_volumes_register( void );
void vk_exposure_volumes_update( const float viewOrigin[3], float dt );

#endif
