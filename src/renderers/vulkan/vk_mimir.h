/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mímir — CUDA/Vulkan interop visualization scaffold (arXiv:2504.20937).
See docs/MIMIR.md.
===========================================================================
*/

#ifndef VK_MIMIR_H
#define VK_MIMIR_H

#include "tr_local.h"

void R_Mimir_Init( void );
void R_Mimir_Shutdown( void );

qboolean R_Mimir_Active( void );
qboolean R_Mimir_RunStep( void );

#endif /* VK_MIMIR_H */
