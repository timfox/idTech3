#pragma once

#include "qcommon/q_shared.h"

/* Mímir CUDA interop (optional USE_MIMIR_CUDA). See mimir_cuda.c */

typedef struct mimirCudaExport_s {
	int         fd;
	uint64_t    size;
	qboolean    valid;
} mimirCudaExport_t;

qboolean MimirCuda_Init( void );
void MimirCuda_Shutdown( void );
qboolean MimirCuda_Available( void );

qboolean MimirCuda_ImportBuffer( const mimirCudaExport_t *exp );
void MimirCuda_ReleaseImport( void );

qboolean MimirCuda_RunBrownian( uint32_t pointCount, float dt, float sigma, uint32_t frameSeed );

const char *MimirCuda_BackendName( void );
