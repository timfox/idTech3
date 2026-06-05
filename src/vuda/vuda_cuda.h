#pragma once

#include "vuda_types.h"

#define VUDA_MAX_STREAMS        3

#define VUDA_STREAM_PHYSICS     0
#define VUDA_STREAM_NEURAL      1
#define VUDA_STREAM_INFERENCE   2

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

qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs, uint64_t waitTimeline,
	uint64_t *outSignalTimeline );

qboolean VudaCuda_BindStream( int streamSlot );
qboolean VudaCuda_UnbindStream( int streamSlot );
qboolean VudaCuda_IsStreamBound( int streamSlot );
int VudaCuda_BoundStreamCount( void );

const char *VudaCuda_BackendName( void );
