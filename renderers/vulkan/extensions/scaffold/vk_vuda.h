#pragma once


#include "vuda/vuda_types.h"

void R_VUDA_Init( void );
void R_VUDA_Shutdown( void );

qboolean R_VUDA_Active( void );
qboolean R_VUDA_InteropReady( void );

qboolean R_VUDA_GetExportBundle( vudaExportBundle_t *out );
qboolean R_VUDA_GetSlotExport( int slot, vudaSlotExport_t *out );

void R_VUDA_TryBuildInterop( void );

void vk_vuda_frame_begin( void );
void vk_vuda_after_queue_submit( void );
qboolean vk_vuda_consume_compute_window( void );
void vk_vuda_notify_cuda_complete( uint64_t value );

