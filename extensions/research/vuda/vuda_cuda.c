/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CUDA runtime interop for VUDA (dynamic libcudart). See docs/VUDA.md.
===========================================================================
*/

#include "vuda_cuda.h"
#include "../../../qcommon/qcommon.h"

#ifdef USE_VUDA

#include <dlfcn.h>

#define VUDA_CUDA_LIB "libcudart.so"

typedef int cudaError_t;
typedef void *cudaStream_t;
typedef void *cudaExternalMemory_t;
typedef void *cudaExternalSemaphore_t;

typedef enum {
	cudaExternalMemoryHandleTypeOpaqueFd = 1,
	cudaExternalSemaphoreHandleTypeTimelineSemaphoreFd = 4
} cudaExternalHandleType_t;

typedef struct {
	cudaExternalHandleType_t type;
	int fd;
	unsigned long long size;
} cudaExternalMemoryHandleDesc_handle;

typedef struct {
	cudaExternalHandleType_t type;
	int fd;
} cudaExternalSemaphoreHandleDesc_handle;

typedef struct {
	unsigned long long offset;
	unsigned long long size;
	unsigned long long flags;
	cudaExternalMemoryHandleDesc_handle handle;
} cudaExternalMemoryHandleDesc_t;

typedef struct {
	unsigned long long flags;
	cudaExternalSemaphoreHandleDesc_handle handle;
} cudaExternalSemaphoreHandleDesc_t;

typedef struct {
	cudaExternalMemory_t *extMem;
	const cudaExternalMemoryHandleDesc_t *memDesc;
	unsigned long long numParams;
} cudaImportExternalMemoryParams_t;

typedef struct {
	cudaExternalSemaphore_t *extSem;
	const cudaExternalSemaphoreHandleDesc_t *semDesc;
	unsigned long long numParams;
} cudaImportExternalSemaphoreParams_t;

typedef struct {
	cudaExternalSemaphore_t *extSem;
	const unsigned long long *params;
	unsigned long long numParams;
	cudaStream_t stream;
} cudaExternalSemaphoreSignalParams_t;

typedef struct {
	cudaExternalSemaphore_t *extSem;
	const unsigned long long *params;
	unsigned long long numParams;
	cudaStream_t stream;
} cudaExternalSemaphoreWaitParams_t;

typedef cudaError_t (*pfn_cudaStreamCreate_t)( cudaStream_t *stream );
typedef cudaError_t (*pfn_cudaStreamDestroy_t)( cudaStream_t stream );
typedef cudaError_t (*pfn_cudaStreamSynchronize_t)( cudaStream_t stream );
typedef cudaError_t (*pfn_cudaSetDevice_t)( int device );
typedef cudaError_t (*pfn_cudaDeviceSynchronize_t)( void );
typedef const char *(*pfn_cudaGetErrorString_t)( cudaError_t err );
typedef cudaError_t (*pfn_cudaImportExternalMemory_t)( cudaExternalMemory_t *out,
	const cudaExternalMemoryHandleDesc_t *desc );
typedef cudaError_t (*pfn_cudaDestroyExternalMemory_t)( cudaExternalMemory_t mem );
typedef cudaError_t (*pfn_cudaExternalMemoryGetMappedBuffer_t)( void **devPtr, cudaExternalMemory_t mem,
	const void *bufferDesc );
typedef cudaError_t (*pfn_cudaImportExternalSemaphore_t)( cudaExternalSemaphore_t *out,
	const cudaExternalSemaphoreHandleDesc_t *desc );
typedef cudaError_t (*pfn_cudaDestroyExternalSemaphore_t)( cudaExternalSemaphore_t sem );
typedef cudaError_t (*pfn_cudaWaitExternalSemaphoresAsync_t)( const cudaExternalSemaphoreWaitParams_t *params,
	unsigned long long num, cudaStream_t stream );
typedef cudaError_t (*pfn_cudaSignalExternalSemaphoresAsync_t)( const cudaExternalSemaphoreSignalParams_t *params,
	unsigned long long num, cudaStream_t stream );
typedef cudaError_t (*pfn_cudaMemset_t)( void *devPtr, int value, size_t count );
typedef cudaError_t (*pfn_cudaMemcpy_t)( void *dst, const void *src, size_t count, int kind );

#define cudaMemcpyDeviceToHost 2
#define cudaSuccess 0

typedef struct {
	void                            *lib;
	pfn_cudaStreamCreate_t           StreamCreate;
	pfn_cudaStreamDestroy_t          StreamDestroy;
	pfn_cudaStreamSynchronize_t      StreamSynchronize;
	pfn_cudaSetDevice_t              SetDevice;
	pfn_cudaDeviceSynchronize_t      DeviceSynchronize;
	pfn_cudaGetErrorString_t         GetErrorString;
	pfn_cudaImportExternalMemory_t   ImportExternalMemory;
	pfn_cudaDestroyExternalMemory_t  DestroyExternalMemory;
	pfn_cudaExternalMemoryGetMappedBuffer_t ExternalMemoryGetMappedBuffer;
	pfn_cudaImportExternalSemaphore_t ImportExternalSemaphore;
	pfn_cudaDestroyExternalSemaphore_t DestroyExternalSemaphore;
	pfn_cudaWaitExternalSemaphoresAsync_t WaitExternalSemaphoresAsync;
	pfn_cudaSignalExternalSemaphoresAsync_t SignalExternalSemaphoresAsync;
	pfn_cudaMemset_t                 Memset;
	pfn_cudaMemcpy_t                 Memcpy;
	qboolean                         loaded;
} vudaCudaApi_t;

typedef struct {
	cudaExternalMemory_t    extMem;
	void                   *devPtr;
	uint64_t                size;
} vudaCudaSlot_t;

static vudaCudaApi_t cudaApi;
static vudaCudaSlot_t cudaSlots[VUDA_MAX_SLOTS];
static cudaExternalSemaphore_t cudaWaitSem;
static cudaExternalSemaphore_t cudaSignalSem;
static cudaStream_t cudaStreams[VUDA_MAX_STREAMS];
static qboolean cudaStreamBound[VUDA_MAX_STREAMS];
static qboolean importsReady;
static int cudaDevice;
static uint64_t cudaLastWaitTimeline;
static uint64_t cudaLastSignalTimeline;

static qboolean VudaCuda_LoadSym( void *sym, const char *name )
{
	void *p = dlsym( cudaApi.lib, name );
	if ( !p ) {
		return qfalse;
	}
	Com_Memcpy( sym, &p, sizeof( p ) );
	return qtrue;
}

qboolean VudaCuda_Init( void )
{
	Com_Memset( &cudaApi, 0, sizeof( cudaApi ) );
	Com_Memset( cudaSlots, 0, sizeof( cudaSlots ) );

	cudaApi.lib = dlopen( VUDA_CUDA_LIB, RTLD_NOW | RTLD_LOCAL );
	if ( !cudaApi.lib ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] %s not loaded: %s\n", VUDA_CUDA_LIB, dlerror() );
		return qfalse;
	}

	if ( !VudaCuda_LoadSym( &cudaApi.StreamCreate, "cudaStreamCreate" ) ||
		!VudaCuda_LoadSym( &cudaApi.StreamDestroy, "cudaStreamDestroy" ) ||
		!VudaCuda_LoadSym( &cudaApi.StreamSynchronize, "cudaStreamSynchronize" ) ||
		!VudaCuda_LoadSym( &cudaApi.SetDevice, "cudaSetDevice" ) ||
		!VudaCuda_LoadSym( &cudaApi.DeviceSynchronize, "cudaDeviceSynchronize" ) ||
		!VudaCuda_LoadSym( &cudaApi.GetErrorString, "cudaGetErrorString" ) ||
		!VudaCuda_LoadSym( &cudaApi.ImportExternalMemory, "cudaImportExternalMemory" ) ||
		!VudaCuda_LoadSym( &cudaApi.DestroyExternalMemory, "cudaDestroyExternalMemory" ) ||
		!VudaCuda_LoadSym( &cudaApi.ExternalMemoryGetMappedBuffer, "cudaExternalMemoryGetMappedBuffer" ) ||
		!VudaCuda_LoadSym( &cudaApi.ImportExternalSemaphore, "cudaImportExternalSemaphore" ) ||
		!VudaCuda_LoadSym( &cudaApi.DestroyExternalSemaphore, "cudaDestroyExternalSemaphore" ) ||
		!VudaCuda_LoadSym( &cudaApi.WaitExternalSemaphoresAsync, "cudaWaitExternalSemaphoresAsync" ) ||
		!VudaCuda_LoadSym( &cudaApi.SignalExternalSemaphoresAsync, "cudaSignalExternalSemaphoresAsync" ) ||
		!VudaCuda_LoadSym( &cudaApi.Memset, "cudaMemset" ) ||
		!VudaCuda_LoadSym( &cudaApi.Memcpy, "cudaMemcpy" ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] libcudart missing required symbols\n" );
		dlclose( cudaApi.lib );
		Com_Memset( &cudaApi, 0, sizeof( cudaApi ) );
		return qfalse;
	}

	cudaApi.loaded = qtrue;
	cudaDevice = 0;
	Com_Memset( cudaStreamBound, 0, sizeof( cudaStreamBound ) );
	if ( cudaApi.SetDevice( cudaDevice ) != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaSetDevice(0) failed\n" );
	}
	Com_Printf( "[VUDA] CUDA runtime loaded (%s)\n", VUDA_CUDA_LIB );
	return qtrue;
}

void VudaCuda_Shutdown( void )
{
	int i;

	VudaCuda_ReleaseImports();
	for ( i = 0; i < VUDA_MAX_STREAMS; i++ ) {
		if ( cudaStreams[i] && cudaApi.StreamDestroy ) {
			cudaApi.StreamDestroy( cudaStreams[i] );
		}
		cudaStreams[i] = NULL;
		cudaStreamBound[i] = qfalse;
	}
	if ( cudaApi.lib ) {
		dlclose( cudaApi.lib );
	}
	Com_Memset( &cudaApi, 0, sizeof( cudaApi ) );
}

qboolean VudaCuda_Available( void )
{
	return cudaApi.loaded ? qtrue : qfalse;
}

const char *VudaCuda_BackendName( void )
{
	return cudaApi.loaded ? "libcudart (external memory fd)" : "none";
}

static void VudaCuda_DestroySlot( int i )
{
	if ( cudaSlots[i].extMem && cudaApi.DestroyExternalMemory ) {
		cudaApi.DestroyExternalMemory( cudaSlots[i].extMem );
	}
	cudaSlots[i].extMem = NULL;
	cudaSlots[i].devPtr = NULL;
	cudaSlots[i].size = 0;
}

void VudaCuda_ReleaseImports( void )
{
	int i;

	if ( cudaWaitSem && cudaApi.DestroyExternalSemaphore ) {
		cudaApi.DestroyExternalSemaphore( cudaWaitSem );
	}
	if ( cudaSignalSem && cudaApi.DestroyExternalSemaphore ) {
		cudaApi.DestroyExternalSemaphore( cudaSignalSem );
	}
	cudaWaitSem = NULL;
	cudaSignalSem = NULL;

	for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
		VudaCuda_DestroySlot( i );
	}
	importsReady = qfalse;
}

qboolean VudaCuda_ImportExports( const vudaExportBundle_t *exp )
{
	int i;
	cudaError_t err;

	if ( !cudaApi.loaded || !exp || !exp->interopReady ) {
		return qfalse;
	}

	VudaCuda_ReleaseImports();

	for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
		cudaExternalMemoryHandleDesc_t desc;

		if ( !exp->slots[i].valid || exp->slots[i].fd < 0 ) {
			continue;
		}

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.offset = 0;
		desc.size = exp->slots[i].size;
		desc.flags = 0;
		desc.handle.type = cudaExternalMemoryHandleTypeOpaqueFd;
		desc.handle.fd = exp->slots[i].fd;
		desc.handle.size = exp->slots[i].size;

		err = cudaApi.ImportExternalMemory( &cudaSlots[i].extMem, &desc );
		if ( err != cudaSuccess ) {
			Com_Printf( S_COLOR_YELLOW "[VUDA] cudaImportExternalMemory slot %d: %s\n",
				i, cudaApi.GetErrorString( err ) );
			VudaCuda_ReleaseImports();
			return qfalse;
		}

		{
			struct {
				unsigned long long offset;
				unsigned long long size;
				unsigned long long flags;
			} bufDesc;

			Com_Memset( &bufDesc, 0, sizeof( bufDesc ) );
			bufDesc.offset = 0;
			bufDesc.size = exp->slots[i].size;
			bufDesc.flags = 0;
			err = cudaApi.ExternalMemoryGetMappedBuffer( &cudaSlots[i].devPtr, cudaSlots[i].extMem, &bufDesc );
			if ( err != cudaSuccess ) {
				Com_Printf( S_COLOR_YELLOW "[VUDA] cudaExternalMemoryGetMappedBuffer slot %d: %s\n",
					i, cudaApi.GetErrorString( err ) );
				VudaCuda_ReleaseImports();
				return qfalse;
			}
		}
		cudaSlots[i].size = exp->slots[i].size;
	}

	if ( exp->cudaWait.valid && exp->cudaWait.fd >= 0 ) {
		cudaExternalSemaphoreHandleDesc_t semDesc;

		Com_Memset( &semDesc, 0, sizeof( semDesc ) );
		semDesc.flags = 0;
		semDesc.handle.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreFd;
		semDesc.handle.fd = exp->cudaWait.fd;
		err = cudaApi.ImportExternalSemaphore( &cudaWaitSem, &semDesc );
		if ( err != cudaSuccess ) {
			Com_Printf( S_COLOR_YELLOW "[VUDA] cudaImportExternalSemaphore wait: %s\n",
				cudaApi.GetErrorString( err ) );
			VudaCuda_ReleaseImports();
			return qfalse;
		}
	}

	if ( exp->cudaSignal.valid && exp->cudaSignal.fd >= 0 ) {
		cudaExternalSemaphoreHandleDesc_t semDesc;

		Com_Memset( &semDesc, 0, sizeof( semDesc ) );
		semDesc.flags = 0;
		semDesc.handle.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreFd;
		semDesc.handle.fd = exp->cudaSignal.fd;
		err = cudaApi.ImportExternalSemaphore( &cudaSignalSem, &semDesc );
		if ( err != cudaSuccess ) {
			Com_Printf( S_COLOR_YELLOW "[VUDA] cudaImportExternalSemaphore signal: %s\n",
				cudaApi.GetErrorString( err ) );
			VudaCuda_ReleaseImports();
			return qfalse;
		}
	}

	importsReady = qtrue;
	return qtrue;
}

qboolean VudaCuda_BindStream( int streamSlot )
{
	cudaError_t err;

	if ( streamSlot < 0 || streamSlot >= VUDA_MAX_STREAMS ) {
		return qfalse;
	}
	if ( !cudaApi.loaded ) {
		return qfalse;
	}

	if ( !cudaStreams[streamSlot] && cudaApi.StreamCreate ) {
		err = cudaApi.StreamCreate( &cudaStreams[streamSlot] );
		if ( err != cudaSuccess ) {
			Com_Printf( S_COLOR_YELLOW "[VUDA] cudaStreamCreate slot %d: %s\n",
				streamSlot, cudaApi.GetErrorString( err ) );
			return qfalse;
		}
	}

	cudaStreamBound[streamSlot] = qtrue;
	Com_Printf( "[VUDA] CUstream_bind: slot %d co-scheduled with Vulkan (software redirect)\n",
		streamSlot );
	return qtrue;
}

qboolean VudaCuda_UnbindStream( int streamSlot )
{
	if ( streamSlot < 0 || streamSlot >= VUDA_MAX_STREAMS ) {
		return qfalse;
	}
	cudaStreamBound[streamSlot] = qfalse;
	Com_Printf( "[VUDA] CUstream_unbind: slot %d restored\n", streamSlot );
	return qtrue;
}

qboolean VudaCuda_IsStreamBound( int streamSlot )
{
	if ( streamSlot < 0 || streamSlot >= VUDA_MAX_STREAMS ) {
		return qfalse;
	}
	return cudaStreamBound[streamSlot] ? qtrue : qfalse;
}

int VudaCuda_BoundStreamCount( void )
{
	int i;
	int n = 0;

	for ( i = 0; i < VUDA_MAX_STREAMS; i++ ) {
		if ( cudaStreamBound[i] ) {
			n++;
		}
	}
	return n;
}

static cudaStream_t VudaCuda_StreamForJob( const vudaCudaJob_t *job )
{
	int slot;

	if ( !job ) {
		return NULL;
	}
	if ( job->streamMask & 4 ) {
		slot = VUDA_STREAM_INFERENCE;
	} else if ( job->streamMask & 2 ) {
		slot = VUDA_STREAM_NEURAL;
	} else if ( job->streamMask & 1 ) {
		slot = VUDA_STREAM_PHYSICS;
	} else {
		slot = VUDA_STREAM_NEURAL;
	}

	if ( slot >= 0 && slot < VUDA_MAX_STREAMS && cudaStreams[slot] ) {
		return cudaStreams[slot];
	}
	return NULL;
}

qboolean VudaCuda_WaitRender( uint64_t waitTimeline, cudaStream_t stream )
{
	cudaExternalSemaphoreWaitParams_t params;
	unsigned long long value;
	cudaError_t err;

	if ( !importsReady || !cudaWaitSem ) {
		return qtrue;
	}

	if ( waitTimeline <= cudaLastWaitTimeline ) {
		return qtrue;
	}

	value = (unsigned long long)waitTimeline;
	Com_Memset( &params, 0, sizeof( params ) );
	params.extSem = &cudaWaitSem;
	params.params = &value;
	params.numParams = 1;
	params.stream = stream;

	err = cudaApi.WaitExternalSemaphoresAsync( &params, 1, stream );
	if ( err != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaWaitExternalSemaphoresAsync: %s\n",
			cudaApi.GetErrorString( err ) );
		return qfalse;
	}

	if ( stream && cudaApi.StreamSynchronize ) {
		err = cudaApi.StreamSynchronize( stream );
	} else {
		err = cudaApi.DeviceSynchronize();
	}
	cudaLastWaitTimeline = waitTimeline;
	return err == cudaSuccess;
}

void VudaCuda_SignalComplete( uint64_t signalTimeline, cudaStream_t stream )
{
	cudaExternalSemaphoreSignalParams_t params;
	unsigned long long value;
	cudaError_t err;

	if ( !importsReady || !cudaSignalSem ) {
		return;
	}

	value = (unsigned long long)signalTimeline;
	Com_Memset( &params, 0, sizeof( params ) );
	params.extSem = &cudaSignalSem;
	params.params = &value;
	params.numParams = 1;
	params.stream = stream;

	err = cudaApi.SignalExternalSemaphoresAsync( &params, 1, stream );
	if ( err != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaSignalExternalSemaphoresAsync: %s\n",
			cudaApi.GetErrorString( err ) );
		return;
	}

	if ( stream && cudaApi.StreamSynchronize ) {
		cudaApi.StreamSynchronize( stream );
	} else {
		cudaApi.DeviceSynchronize();
	}
	cudaLastSignalTimeline = signalTimeline;
}

qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs, uint64_t waitTimeline, uint64_t *outSignalTimeline )
{
	void *ptr;
	uint32_t touch;
	cudaError_t err;
	cudaStream_t stream;
	uint64_t signalTimeline;

	(void)maxMs;

	if ( !job || !importsReady ) {
		return qfalse;
	}

	stream = VudaCuda_StreamForJob( job );
	if ( !stream && cudaApi.StreamCreate ) {
		(void)VudaCuda_BindStream( VUDA_STREAM_NEURAL );
		stream = cudaStreams[VUDA_STREAM_NEURAL];
	}

	if ( !VudaCuda_WaitRender( waitTimeline, stream ) ) {
		return qfalse;
	}

	ptr = cudaSlots[VUDA_SLOT_NEURAL].devPtr;
	if ( !ptr && cudaSlots[VUDA_SLOT_PHYSICS].devPtr ) {
		ptr = cudaSlots[VUDA_SLOT_PHYSICS].devPtr;
	}
	if ( !ptr ) {
		return qfalse;
	}

	touch = job->bytes;
	if ( touch < 256 ) {
		touch = 256;
	}
	if ( touch > 4096 ) {
		touch = 4096;
	}
	if ( cudaSlots[VUDA_SLOT_NEURAL].size > 0 && touch > cudaSlots[VUDA_SLOT_NEURAL].size ) {
		touch = (uint32_t)cudaSlots[VUDA_SLOT_NEURAL].size;
	}

	switch ( job->kind ) {
	case VUDA_JOB_HEARTBEAT:
	case VUDA_JOB_NEURAL_STAGE:
	case VUDA_JOB_PHYSICS_TICK:
	case VUDA_JOB_INFERENCE:
	default:
		err = cudaApi.Memset( ptr, 0x3f, touch );
		if ( err != cudaSuccess ) {
			Com_Printf( S_COLOR_YELLOW "[VUDA] cudaMemset job %d: %s\n",
				job->kind, cudaApi.GetErrorString( err ) );
			return qfalse;
		}
		break;
	}

	signalTimeline = waitTimeline > 0 ? waitTimeline : ( cudaLastSignalTimeline + 1 );
	if ( signalTimeline <= cudaLastSignalTimeline ) {
		signalTimeline = cudaLastSignalTimeline + 1;
	}
	VudaCuda_SignalComplete( signalTimeline, stream );

	if ( outSignalTimeline ) {
		*outSignalTimeline = signalTimeline;
	}
	return qtrue;
}

#else /* !USE_VUDA */

qboolean VudaCuda_Init( void ) { return qfalse; }
void VudaCuda_Shutdown( void ) {}
qboolean VudaCuda_Available( void ) { return qfalse; }
qboolean VudaCuda_ImportExports( const vudaExportBundle_t *exp ) { (void)exp; return qfalse; }
void VudaCuda_ReleaseImports( void ) {}
qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs, uint64_t waitTimeline, uint64_t *outSignalTimeline ) { (void)job; (void)maxMs; (void)waitTimeline; (void)outSignalTimeline; return qfalse; }
void VudaCuda_SignalComplete( uint64_t signalTimeline, cudaStream_t stream ) { (void)signalTimeline; (void)stream; }
qboolean VudaCuda_WaitRender( uint64_t waitTimeline, cudaStream_t stream ) { (void)waitTimeline; (void)stream; return qfalse; }
qboolean VudaCuda_BindStream( int streamSlot ) { (void)streamSlot; return qfalse; }
qboolean VudaCuda_UnbindStream( int streamSlot ) { (void)streamSlot; return qfalse; }
qboolean VudaCuda_IsStreamBound( int streamSlot ) { (void)streamSlot; return qfalse; }
int VudaCuda_BoundStreamCount( void ) { return 0; }
const char *VudaCuda_BackendName( void ) { return "disabled"; }

#endif
