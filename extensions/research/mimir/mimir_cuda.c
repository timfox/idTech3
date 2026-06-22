/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mímir CUDA interop — dynamic libcudart import (optional USE_MIMIR_CUDA).
Brownian simulation runs on Vulkan compute in v1; this module wires
VK_KHR_external_memory_fd → cudaImportExternalMemory.
===========================================================================
*/

#include "mimir_cuda.h"
#include "../../../qcommon/qcommon.h"

#ifdef USE_MIMIR_CUDA

#include <dlfcn.h>

typedef int cudaError_t;
typedef void *cudaExternalMemory_t;

typedef enum {
	cudaExternalMemoryHandleTypeOpaqueFd = 1
} cudaExternalHandleType_t;

typedef struct {
	cudaExternalHandleType_t type;
	int fd;
	unsigned long long size;
} cudaExternalMemoryHandleDesc_handle;

typedef struct {
	unsigned long long offset;
	unsigned long long size;
	unsigned long long flags;
	cudaExternalMemoryHandleDesc_handle handle;
} cudaExternalMemoryHandleDesc_t;

typedef cudaError_t (*pfn_cudaImportExternalMemory_t)( cudaExternalMemory_t *out,
	const cudaExternalMemoryHandleDesc_t *desc );
typedef cudaError_t (*pfn_cudaDestroyExternalMemory_t)( cudaExternalMemory_t mem );
typedef cudaError_t (*pfn_cudaExternalMemoryGetMappedBuffer_t)( void **devPtr, cudaExternalMemory_t mem,
	const void *bufferDesc );
typedef const char *(*pfn_cudaGetErrorString_t)( cudaError_t err );

#define cudaSuccess 0

typedef struct {
	void *lib;
	pfn_cudaImportExternalMemory_t ImportExternalMemory;
	pfn_cudaDestroyExternalMemory_t DestroyExternalMemory;
	pfn_cudaExternalMemoryGetMappedBuffer_t ExternalMemoryGetMappedBuffer;
	pfn_cudaGetErrorString_t GetErrorString;
	cudaExternalMemory_t extMem;
	void *devPtr;
	qboolean loaded;
	qboolean imported;
} mimirCudaApi_t;

static mimirCudaApi_t mimir_cuda;

static qboolean MimirCuda_LoadApi( void )
{
	if ( mimir_cuda.loaded ) {
		return qtrue;
	}
	mimir_cuda.lib = dlopen( "libcudart.so", RTLD_LAZY | RTLD_LOCAL );
	if ( !mimir_cuda.lib ) {
		return qfalse;
	}
	mimir_cuda.ImportExternalMemory = (pfn_cudaImportExternalMemory_t)dlsym( mimir_cuda.lib, "cudaImportExternalMemory" );
	mimir_cuda.DestroyExternalMemory = (pfn_cudaDestroyExternalMemory_t)dlsym( mimir_cuda.lib, "cudaDestroyExternalMemory" );
	mimir_cuda.ExternalMemoryGetMappedBuffer = (pfn_cudaExternalMemoryGetMappedBuffer_t)dlsym( mimir_cuda.lib,
		"cudaExternalMemoryGetMappedBuffer" );
	mimir_cuda.GetErrorString = (pfn_cudaGetErrorString_t)dlsym( mimir_cuda.lib, "cudaGetErrorString" );
	if ( !mimir_cuda.ImportExternalMemory || !mimir_cuda.DestroyExternalMemory ||
		!mimir_cuda.ExternalMemoryGetMappedBuffer ) {
		dlclose( mimir_cuda.lib );
		Com_Memset( &mimir_cuda, 0, sizeof( mimir_cuda ) );
		return qfalse;
	}
	mimir_cuda.loaded = qtrue;
	return qtrue;
}

qboolean MimirCuda_Init( void )
{
	return MimirCuda_LoadApi();
}

void MimirCuda_Shutdown( void )
{
	MimirCuda_ReleaseImport();
	if ( mimir_cuda.lib ) {
		dlclose( mimir_cuda.lib );
	}
	Com_Memset( &mimir_cuda, 0, sizeof( mimir_cuda ) );
}

qboolean MimirCuda_Available( void )
{
	return mimir_cuda.loaded ? qtrue : qfalse;
}

qboolean MimirCuda_ImportBuffer( const mimirCudaExport_t *exp )
{
	cudaExternalMemoryHandleDesc_t desc;
	cudaError_t err;
	struct {
		unsigned long long offset;
		unsigned long long size;
		unsigned long long flags;
	} bufDesc;

	if ( !exp || !exp->valid || exp->fd < 0 || exp->size == 0 ) {
		return qfalse;
	}
	if ( !MimirCuda_LoadApi() ) {
		return qfalse;
	}
	MimirCuda_ReleaseImport();

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.handle.type = cudaExternalMemoryHandleTypeOpaqueFd;
	desc.handle.fd = exp->fd;
	desc.size = exp->size;

	err = mimir_cuda.ImportExternalMemory( &mimir_cuda.extMem, &desc );
	if ( err != cudaSuccess ) {
		Com_Printf( S_COLOR_YELLOW "[Mímir] cudaImportExternalMemory failed: %s\n",
			mimir_cuda.GetErrorString ? mimir_cuda.GetErrorString( err ) : "unknown" );
		return qfalse;
	}

	Com_Memset( &bufDesc, 0, sizeof( bufDesc ) );
	bufDesc.offset = 0;
	bufDesc.size = exp->size;

	err = mimir_cuda.ExternalMemoryGetMappedBuffer( &mimir_cuda.devPtr, mimir_cuda.extMem, &bufDesc );
	if ( err != cudaSuccess ) {
		mimir_cuda.DestroyExternalMemory( mimir_cuda.extMem );
		mimir_cuda.extMem = NULL;
		return qfalse;
	}

	mimir_cuda.imported = qtrue;
	return qtrue;
}

void MimirCuda_ReleaseImport( void )
{
	if ( mimir_cuda.extMem && mimir_cuda.DestroyExternalMemory ) {
		mimir_cuda.DestroyExternalMemory( mimir_cuda.extMem );
	}
	mimir_cuda.extMem = NULL;
	mimir_cuda.devPtr = NULL;
	mimir_cuda.imported = qfalse;
}

qboolean MimirCuda_RunBrownian( uint32_t pointCount, float dt, float sigma, uint32_t frameSeed )
{
	(void)pointCount;
	(void)dt;
	(void)sigma;
	(void)frameSeed;
	/* v1: Brownian kernel on Vulkan compute; CUDA mapped buffer validated at import only */
	return qfalse;
}

const char *MimirCuda_BackendName( void )
{
	return mimir_cuda.loaded ? "libcudart (dlopen)" : "unavailable";
}

#else /* !USE_MIMIR_CUDA */

qboolean MimirCuda_Init( void ) { return qfalse; }
void MimirCuda_Shutdown( void ) { }
qboolean MimirCuda_Available( void ) { return qfalse; }
qboolean MimirCuda_ImportBuffer( const mimirCudaExport_t *exp ) { (void)exp; return qfalse; }
void MimirCuda_ReleaseImport( void ) { }
qboolean MimirCuda_RunBrownian( uint32_t pointCount, float dt, float sigma, uint32_t frameSeed )
{
	(void)pointCount; (void)dt; (void)sigma; (void)frameSeed;
	return qfalse;
}
const char *MimirCuda_BackendName( void ) { return "disabled"; }

#endif
