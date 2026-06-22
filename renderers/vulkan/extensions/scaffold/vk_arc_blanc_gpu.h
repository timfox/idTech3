/*
 * Arc Blanc GPU ocean compute (Tessendorf FFT cascades).
 */
#pragma once

typedef struct arcBlancGpuParams_s arcBlancGpuParams_t;

void VK_ArcBlancGpu_Init( void );
void VK_ArcBlancGpu_Shutdown( void );
qboolean RE_ArcBlancGpuOceanStep( const arcBlancGpuParams_t *params );
