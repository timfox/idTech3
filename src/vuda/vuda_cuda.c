/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CUDA runtime interop for VUDA (dynamic libcudart). See docs/VUDA.md.
===========================================================================
*/

#include "vuda_cuda.h"
#include "../qcommon/qcommon.h"

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
static qboolean importsReady;
static int cudaDevice;

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

	if ( !VudaCuda_LoadSym( &cudaApi.SetDevice, "cudaSetDevice" ) ||
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
	if ( cudaApi.SetDevice( cudaDevice ) != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaSetDevice(0) failed\n" );
	}
	Com_Printf( "[VUDA] CUDA runtime loaded (%s)\n", VUDA_CUDA_LIB );
	return qtrue;
}

void VudaCuda_Shutdown( void )
{
	VudaCuda_ReleaseImports();
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

qboolean VudaCuda_WaitRender( int timeoutMs )
{
	cudaExternalSemaphoreWaitParams_t params;
	unsigned long long value;
	cudaError_t err;

	(void)timeoutMs;

	if ( !importsReady || !cudaWaitSem ) {
		return qtrue;
	}

	value = 0;
	Com_Memset( &params, 0, sizeof( params ) );
	params.extSem = &cudaWaitSem;
	params.params = &value;
	params.numParams = 1;
	params.stream = NULL;

	err = cudaApi.WaitExternalSemaphoresAsync( &params, 1, NULL );
	if ( err != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaWaitExternalSemaphoresAsync: %s\n",
			cudaApi.GetErrorString( err ) );
		return qfalse;
	}
	return cudaApi.DeviceSynchronize() == cudaSuccess;
}

void VudaCuda_SignalComplete( void )
{
	cudaExternalSemaphoreSignalParams_t params;
	unsigned long long value;
	cudaError_t err;

	if ( !importsReady || !cudaSignalSem ) {
		return;
	}

	value = 1;
	Com_Memset( &params, 0, sizeof( params ) );
	params.extSem = &cudaSignalSem;
	params.params = &value;
	params.numParams = 1;
	params.stream = NULL;

	err = cudaApi.SignalExternalSemaphoresAsync( &params, 1, NULL );
	if ( err != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] cudaSignalExternalSemaphoresAsync: %s\n",
			cudaApi.GetErrorString( err ) );
	}
	cudaApi.DeviceSynchronize();
}

qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs )
{
	void *ptr;
	uint32_t touch;
	cudaError_t err;
	int start;

	(void)maxMs;

	if ( !job || !importsReady ) {
		return qfalse;
	}

	start = Sys_Milliseconds();

	if ( !VudaCuda_WaitRender( 8 ) ) {
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

	(void)start;
	VudaCuda_SignalComplete();
	return qtrue;
}

#else /* !USE_VUDA */

qboolean VudaCuda_Init( void ) { return qfalse; }
void VudaCuda_Shutdown( void ) {}
qboolean VudaCuda_Available( void ) { return qfalse; }
qboolean VudaCuda_ImportExports( const vudaExportBundle_t *exp ) { (void)exp; return qfalse; }
void VudaCuda_ReleaseImports( void ) {}
qboolean VudaCuda_RunJob( const vudaCudaJob_t *job, int maxMs ) { (void)job; (void)maxMs; return qfalse; }
void VudaCuda_SignalComplete( void ) {}
qboolean VudaCuda_WaitRender( int timeoutMs ) { (void)timeoutMs; return qfalse; }
const char *VudaCuda_BackendName( void ) { return "disabled"; }

#endif
