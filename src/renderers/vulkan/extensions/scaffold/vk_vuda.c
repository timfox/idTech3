/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VUDA — CUDA-Vulkan spatial multiplexing scaffold (external memory + timeline sync).
See docs/VUDA.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vuda.h"
#include "vk_util.h"

#ifdef USE_VUDA

#include <unistd.h>

static cvar_t *r_vuda;
static cvar_t *r_vuda_mode;
static cvar_t *r_vuda_slotMb;
static cvar_t *r_vuda_mux;
static cvar_t *r_vuda_computeMs;
static cvar_t *r_vuda_coStreamMask;
static cvar_t *r_vuda_syncCuda;
static cvar_t *r_vuda_debug;

static qboolean vuda_loaded;
static vudaExportBundle_t vuda_exports;

static void VUDA_ClearExports( void )
{
	int i;

	Com_Memset( &vuda_exports, 0, sizeof( vuda_exports ) );
	for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
		vuda_exports.slots[i].fd = -1;
	}
	vuda_exports.cudaWait.fd = -1;
	vuda_exports.cudaSignal.fd = -1;
}

static void VUDA_DestroySlot( int slot )
{
	if ( slot < 0 || slot >= VUDA_MAX_SLOTS ) {
		return;
	}
	if ( vuda_exports.slots[slot].fd >= 0 ) {
		close( vuda_exports.slots[slot].fd );
		vuda_exports.slots[slot].fd = -1;
	}
	if ( vk.vuda.slot_buffer[slot] != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vuda.slot_buffer[slot], NULL );
		vk.vuda.slot_buffer[slot] = VK_NULL_HANDLE;
	}
	if ( vk.vuda.slot_memory[slot] != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vuda.slot_memory[slot], NULL );
		vk.vuda.slot_memory[slot] = VK_NULL_HANDLE;
	}
	vk.vuda.slot_valid[slot] = qfalse;
	vuda_exports.slots[slot].valid = qfalse;
}

static void VUDA_DestroySemaphores( void )
{
	if ( vuda_exports.cudaWait.fd >= 0 ) {
		close( vuda_exports.cudaWait.fd );
		vuda_exports.cudaWait.fd = -1;
	}
	if ( vuda_exports.cudaSignal.fd >= 0 ) {
		close( vuda_exports.cudaSignal.fd );
		vuda_exports.cudaSignal.fd = -1;
	}
	if ( vk.vuda.cuda_wait_sem != VK_NULL_HANDLE ) {
		qvkDestroySemaphore( vk.device, vk.vuda.cuda_wait_sem, NULL );
		vk.vuda.cuda_wait_sem = VK_NULL_HANDLE;
	}
	if ( vk.vuda.cuda_signal_sem != VK_NULL_HANDLE ) {
		qvkDestroySemaphore( vk.device, vk.vuda.cuda_signal_sem, NULL );
		vk.vuda.cuda_signal_sem = VK_NULL_HANDLE;
	}
}

static void VUDA_ClearGpu( void )
{
	int i;

	for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
		VUDA_DestroySlot( i );
	}
	VUDA_DestroySemaphores();
	vk.vuda.interopReady = qfalse;
	vk.vuda.computeWindowOpen = qfalse;
	vk.vuda.renderTimeline = 0;
	vk.vuda.cudaTimeline = 0;
	vk.vudaAllocated = qfalse;
	vuda_loaded = qfalse;
	VUDA_ClearExports();
}

static qboolean VUDA_FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags props, uint32_t *outIndex )
{
	VkPhysicalDeviceMemoryProperties memProps;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memProps );
	for ( i = 0; i < memProps.memoryTypeCount; i++ ) {
		if ( ( typeBits & ( 1u << i ) ) &&
			( memProps.memoryTypes[i].propertyFlags & props ) == props ) {
			*outIndex = i;
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean VUDA_CreateExportSlot( int slot, VkDeviceSize size )
{
	VkBufferCreateInfo buf_ci;
	VkExternalMemoryBufferCreateInfo ext_buf;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	VkExportMemoryAllocateInfo export_alloc;
	VkMemoryFdPropertiesKHR fd_props;
	VkMemoryGetFdInfoKHR get_fd;
	uint32_t memType;
	int fd;

	if ( slot < 0 || slot >= VUDA_MAX_SLOTS ) {
		return qfalse;
	}

	VUDA_DestroySlot( slot );

	Com_Memset( &ext_buf, 0, sizeof( ext_buf ) );
	ext_buf.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	ext_buf.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.pNext = &ext_buf;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.vuda.slot_buffer[slot] ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.vuda.slot_buffer[slot], &mem_req );

	if ( !VUDA_FindMemoryType( mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memType ) ) {
		ri.Printf( PRINT_WARNING, "[VUDA] No DEVICE_LOCAL memory type for slot %d\n", slot );
		VUDA_DestroySlot( slot );
		return qfalse;
	}

	Com_Memset( &export_alloc, 0, sizeof( export_alloc ) );
	export_alloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	export_alloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.pNext = &export_alloc;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = memType;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.vuda.slot_memory[slot] ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.vuda.slot_buffer[slot], vk.vuda.slot_memory[slot], 0 ) );

	if ( !qvkGetMemoryFdKHR ) {
		ri.Printf( PRINT_WARNING, "[VUDA] qvkGetMemoryFdKHR unavailable\n" );
		VUDA_DestroySlot( slot );
		return qfalse;
	}

	Com_Memset( &get_fd, 0, sizeof( get_fd ) );
	get_fd.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
	get_fd.memory = vk.vuda.slot_memory[slot];
	get_fd.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

	fd = -1;
	if ( qvkGetMemoryFdKHR( vk.device, &get_fd, &fd ) != VK_SUCCESS || fd < 0 ) {
		ri.Printf( PRINT_WARNING, "[VUDA] GetMemoryFdKHR failed for slot %d\n", slot );
		VUDA_DestroySlot( slot );
		return qfalse;
	}

	Com_Memset( &fd_props, 0, sizeof( fd_props ) );
	fd_props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	if ( qvkGetMemoryFdPropertiesKHR &&
		qvkGetMemoryFdPropertiesKHR( vk.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, fd, &fd_props ) == VK_SUCCESS ) {
		vuda_exports.slots[slot].memoryTypeIndex = fd_props.memoryTypeBits;
	} else {
		vuda_exports.slots[slot].memoryTypeIndex = memType;
	}

	vk.vuda.slot_size[slot] = size;
	vk.vuda.slot_valid[slot] = qtrue;
	vuda_exports.slots[slot].fd = fd;
	vuda_exports.slots[slot].size = (uint64_t)size;
	vuda_exports.slots[slot].valid = qtrue;
	return qtrue;
}

static qboolean VUDA_CreateTimelineSem( VkSemaphore *outSem, int *outFd, const char *label )
{
	VkSemaphoreTypeCreateInfo type_ci;
	VkExportSemaphoreCreateInfo export_ci;
	VkSemaphoreCreateInfo sem_ci;
	VkSemaphoreGetFdInfoKHR get_fd;
	int fd;

	Com_Memset( &type_ci, 0, sizeof( type_ci ) );
	type_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	type_ci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	type_ci.initialValue = 0;

	Com_Memset( &export_ci, 0, sizeof( export_ci ) );
	export_ci.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
	export_ci.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

	Com_Memset( &sem_ci, 0, sizeof( sem_ci ) );
	sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	sem_ci.pNext = &type_ci;
	type_ci.pNext = &export_ci;

	VK_CHECK( qvkCreateSemaphore( vk.device, &sem_ci, NULL, outSem ) );
	SET_OBJECT_NAME( *outSem, label, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );

	if ( !qvkGetSemaphoreFdKHR ) {
		qvkDestroySemaphore( vk.device, *outSem, NULL );
		*outSem = VK_NULL_HANDLE;
		return qfalse;
	}

	Com_Memset( &get_fd, 0, sizeof( get_fd ) );
	get_fd.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
	get_fd.semaphore = *outSem;
	get_fd.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

	fd = -1;
	if ( qvkGetSemaphoreFdKHR( vk.device, &get_fd, &fd ) != VK_SUCCESS || fd < 0 ) {
		qvkDestroySemaphore( vk.device, *outSem, NULL );
		*outSem = VK_NULL_HANDLE;
		return qfalse;
	}

	*outFd = fd;
	return qtrue;
}

static qboolean VUDA_BuildInterop( void )
{
	int slotMb;
	int i;
	VkDeviceSize slotBytes;

	if ( !vk.vudaInteropCapable ) {
		ri.Printf( PRINT_WARNING, "[VUDA] Device lacks external memory/semaphore fd + timeline\n" );
		return qfalse;
	}

	VUDA_ClearGpu();
	VUDA_ClearExports();

	slotMb = r_vuda_slotMb ? r_vuda_slotMb->integer : 64;
	if ( slotMb < 4 ) {
		slotMb = 4;
	}
	if ( slotMb > 512 ) {
		slotMb = 512;
	}
	slotBytes = (VkDeviceSize)slotMb * 1024u * 1024u;

	for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
		if ( !VUDA_CreateExportSlot( i, slotBytes ) ) {
			return qfalse;
		}
	}

	if ( !VUDA_CreateTimelineSem( &vk.vuda.cuda_wait_sem, &vuda_exports.cudaWait.fd, "vuda_cuda_wait" ) ) {
		return qfalse;
	}
	vuda_exports.cudaWait.valid = qtrue;

	if ( !VUDA_CreateTimelineSem( &vk.vuda.cuda_signal_sem, &vuda_exports.cudaSignal.fd, "vuda_cuda_signal" ) ) {
		return qfalse;
	}
	vuda_exports.cudaSignal.valid = qtrue;

	vk.vuda.interopReady = qtrue;
	vuda_exports.interopReady = qtrue;
	vk.vudaAllocated = qtrue;
	vuda_loaded = qtrue;

	ri.Printf( PRINT_ALL,
		"[VUDA] Interop ready: %d slots x %d MiB, timeline semaphores (mux=%s)\n",
		VUDA_MAX_SLOTS, slotMb,
		( r_vuda_mux && r_vuda_mux->integer ) ? "on" : "off" );
	return qtrue;
}

void R_VUDA_Init( void )
{
	r_vuda = ri.Cvar_Get( "r_vuda", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_vuda_mode = ri.Cvar_Get( "r_vuda_mode", "1", CVAR_ARCHIVE_ND );
	r_vuda_slotMb = ri.Cvar_Get( "r_vuda_slotMb", "64", CVAR_ARCHIVE_ND );
	r_vuda_mux = ri.Cvar_Get( "r_vuda_mux", "1", CVAR_ARCHIVE_ND );
	r_vuda_computeMs = ri.Cvar_Get( "r_vuda_computeMs", "2", CVAR_ARCHIVE_ND );
	r_vuda_coStreamMask = ri.Cvar_Get( "r_vuda_coStreamMask", "7", CVAR_ARCHIVE_ND );
	r_vuda_syncCuda = ri.Cvar_Get( "r_vuda_syncCuda", "1", CVAR_ARCHIVE_ND );
	r_vuda_debug = ri.Cvar_Get( "r_vuda_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_vuda, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_vuda_mode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_vuda_mode,
		"VUDA mode: 0=off, 1=interop (export/import fd), 2=spatial_mux (pipelined, no frame-begin CUDA wait)." );
	ri.Cvar_CheckRange( r_vuda_slotMb, "4", "512", CV_INTEGER );
	ri.Cvar_CheckRange( r_vuda_mux, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_vuda_computeMs, "0", "16", CV_INTEGER );
	ri.Cvar_CheckRange( r_vuda_syncCuda, "0", "1", CV_INTEGER );

	ri.Cvar_SetDescription( r_vuda,
		"VUDA CUDA-Vulkan spatial multiplexing (0=off, 1=on; latched, vid_restart)." );
	ri.Cvar_SetDescription( r_vuda_mux,
		"Open compute window after queue submit (spatial mux heuristic)." );
	ri.Cvar_SetDescription( r_vuda_coStreamMask,
		"Bitmask of co-scheduled CUDA streams (1=physics 2=neural 4=inference)." );

	if ( r_vuda->integer && vk.vudaInteropCapable ) {
		ri.Printf( PRINT_ALL,
			"[VUDA] Enabled mode=%d (experimental). See docs/VUDA.md — pair with cl_vuda 1 after vid_restart\n",
			r_vuda_mode ? r_vuda_mode->integer : 1 );
	}
}

void R_VUDA_Shutdown( void )
{
	VUDA_ClearGpu();
}

qboolean R_VUDA_Active( void )
{
	return ( r_vuda && r_vuda->integer && vuda_loaded && vk.vuda.interopReady ) ? qtrue : qfalse;
}

qboolean R_VUDA_InteropReady( void )
{
	return vk.vuda.interopReady;
}

qboolean R_VUDA_GetExportBundle( vudaExportBundle_t *out )
{
	if ( !out || !R_VUDA_InteropReady() ) {
		return qfalse;
	}
	*out = vuda_exports;
	out->renderTimeline = vk.vuda.renderTimeline;
	out->cudaTimeline = vk.vuda.cudaTimeline;
	return qtrue;
}

qboolean R_VUDA_GetSlotExport( int slot, vudaSlotExport_t *out )
{
	if ( !out || slot < 0 || slot >= VUDA_MAX_SLOTS || !R_VUDA_InteropReady() ) {
		return qfalse;
	}
	*out = vuda_exports.slots[slot];
	return out->valid;
}

void R_VUDA_TryBuildInterop( void )
{
	if ( !r_vuda || !r_vuda->integer ) {
		return;
	}
	if ( vuda_loaded ) {
		return;
	}
	if ( !vk.device || vk.device_lost ) {
		return;
	}
	(void)VUDA_BuildInterop();
}

void vk_vuda_frame_begin( void )
{
	vk.vuda.computeWindowOpen = qfalse;

	if ( !R_VUDA_Active() ) {
		return;
	}

	if ( r_vuda_syncCuda && r_vuda_syncCuda->integer && qvkWaitSemaphoresKHR && vk.vuda.cuda_signal_sem != VK_NULL_HANDLE ) {
		VkSemaphoreWaitInfo wait_info;
		uint64_t value;

		/* Mode 2 (spatial_mux): pipelined overlap — skip blocking wait at frame begin. */
		if ( r_vuda_mode && r_vuda_mode->integer >= 2 ) {
			return;
		}

		value = vk.vuda.cudaTimeline;
		Com_Memset( &wait_info, 0, sizeof( wait_info ) );
		wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores = &vk.vuda.cuda_signal_sem;
		wait_info.pValues = &value;
		(void)qvkWaitSemaphoresKHR( vk.device, &wait_info, UINT64_MAX );
	}
}

void vk_vuda_after_queue_submit( void )
{
	if ( !R_VUDA_Active() ) {
		return;
	}
	if ( !r_vuda_mux || !r_vuda_mux->integer ) {
		return;
	}

	vk.vuda.computeWindowOpen = qtrue;
	vk.vuda.renderTimeline++;

	if ( qvkSignalSemaphoreKHR && vk.vuda.cuda_wait_sem != VK_NULL_HANDLE ) {
		VkSemaphoreSignalInfo signal_info;

		Com_Memset( &signal_info, 0, sizeof( signal_info ) );
		signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
		signal_info.semaphore = vk.vuda.cuda_wait_sem;
		signal_info.value = vk.vuda.renderTimeline;
		(void)qvkSignalSemaphoreKHR( vk.device, &signal_info );
	}

	if ( r_vuda_debug && r_vuda_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[VUDA] compute window open (timeline=%llu)\n",
			(unsigned long long)vk.vuda.renderTimeline );
	}
}

qboolean vk_vuda_consume_compute_window( void )
{
	qboolean open;

	open = vk.vuda.computeWindowOpen;
	vk.vuda.computeWindowOpen = qfalse;
	return open;
}

void vk_vuda_notify_cuda_complete( uint64_t value )
{
	vk.vuda.cudaTimeline = value;
}

#else /* !USE_VUDA */

void R_VUDA_Init( void ) {}
void R_VUDA_Shutdown( void ) {}
qboolean R_VUDA_Active( void ) { return qfalse; }
qboolean R_VUDA_InteropReady( void ) { return qfalse; }
qboolean R_VUDA_GetExportBundle( vudaExportBundle_t *out ) { (void)out; return qfalse; }
qboolean R_VUDA_GetSlotExport( int slot, vudaSlotExport_t *out ) { (void)slot; (void)out; return qfalse; }
void R_VUDA_TryBuildInterop( void ) {}
void vk_vuda_frame_begin( void ) {}
void vk_vuda_after_queue_submit( void ) {}
qboolean vk_vuda_consume_compute_window( void ) { return qfalse; }
void vk_vuda_notify_cuda_complete( uint64_t value ) { (void)value; }

#endif
