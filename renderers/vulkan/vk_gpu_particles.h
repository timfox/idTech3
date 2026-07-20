/*
===========================================================================
Raster Ultra 1.4 — GPU particles (compute sim + soft depth-aware splat).
===========================================================================
*/
#ifndef VK_GPU_PARTICLES_H
#define VK_GPU_PARTICLES_H

#include "../common/tr_types.h"

#define VK_GP_MAX_PARTICLES 4096

typedef struct {
	float posLife[4];
	float velSize[4];
	float colorEmi[4];
	float meta[4];
} vk_gp_particle_t;

void vk_gpu_particles_init( void );
void vk_gpu_particles_shutdown( void );
void vk_gpu_particles_frame_begin( void );
void vk_gpu_particles_apply_after_geometry( void );
qboolean vk_gpu_particles_active( void );
qboolean vk_gpu_particles_mapped( void **mapped, uint32_t *liveCount );

#endif
