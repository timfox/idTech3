#pragma once

#include "vuda_types.h"

typedef struct vudaCudaJob_s {
	int         kind;
	uint32_t    streamMask;
	uint32_t    bytes;
} vudaCudaJob_t;

qboolean VudaCuda_Init( void );
void VudaCuda_Shutdown( void );
qboolean VudaCuda_Available( void );

qboolean VudaCuda_ImportExports( const vudaExportBundle_t *exp );
void VudaCuda_ReleaseImports( void );

qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs );
void VudaCuda_SignalComplete( void );
qboolean VudaCuda_WaitRender( int timeoutMs );

const char *VudaCuda_BackendName( void );
