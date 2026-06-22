#pragma once

#include "../../../common/tr_types.h"

typedef struct arcBlancGpuParams_s arcBlancGpuParams_t;

void RE_ArcBlancUploadHeightMap( const byte *rgba, int width, int height );
qboolean RE_ArcBlancGpuOceanStep( const arcBlancGpuParams_t *params );
void R_ArcBlanc_Init( void );
void R_ArcBlanc_AddSurfaces( void );
void VK_ArcBlanc_Shutdown( void );
