/*
===========================================================================
Scene depth contract — reversed-Z depth writer/reader tracking.
Foundation Consolidation. See docs/GPU_DRIVEN_RENDERING.md (Hi-Z) and depth notes.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_depth_contract.h"
#include "vk_hiz.h"

static cvar_t *r_depthDebug;
static char s_depthFirstWriter[48];
static char s_depthLastWriter[48];
static char s_depthReaders[8][48];
static uint32_t s_depthReaderCount;
static qboolean s_depthWritten;
static qboolean s_cmdsRegistered;

static void VK_Depth_Status_f( void )
{
	uint32_t i;

	ri.Printf( PRINT_ALL, "======== Depth Contract ========\n" );
	ri.Printf( PRINT_ALL, "convention: reversed-Z, compare GREATER_OR_EQUAL, clear=0\n" );
	ri.Printf( PRINT_ALL, "r_depthDebug=%d written=%s\n",
		r_depthDebug ? r_depthDebug->integer : 0,
		s_depthWritten ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "firstWriter=%s lastWriter=%s\n",
		s_depthFirstWriter[0] ? s_depthFirstWriter : "(none)",
		s_depthLastWriter[0] ? s_depthLastWriter : "(none)" );
	ri.Printf( PRINT_ALL, "extent=%ux%u format=%u image=%p\n",
		vk.renderWidth, vk.renderHeight, (unsigned)vk.depth_format,
		(void *)(uintptr_t)vk.depth_image );
	for ( i = 0; i < s_depthReaderCount; i++ ) {
		ri.Printf( PRINT_ALL, "  reader[%u]=%s\n", i, s_depthReaders[i] );
	}
	vk_hiz_status_f();
}

void vk_depth_contract_register( void )
{
	r_depthDebug = ri.Cvar_Get( "r_depthDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_depthDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_depthDebug,
		"Depth contract debug: 0 off, 1 writers, 2 previous-depth, 3 Hi-Z link, 4 full status." );
	ri.Cvar_SetGroup( r_depthDebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "depth_status", VK_Depth_Status_f );
		s_cmdsRegistered = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][depth] depth contract ready (reversed-Z GREATER_OR_EQUAL); depth_status, r_depthDebug\n" );
}

void vk_depth_contract_begin_frame( void )
{
	s_depthFirstWriter[0] = '\0';
	s_depthLastWriter[0] = '\0';
	s_depthWritten = qfalse;
	s_depthReaderCount = 0u;
	Com_Memset( s_depthReaders, 0, sizeof( s_depthReaders ) );
}

void vk_depth_contract_note_writer( const char *passName )
{
	if ( !passName || !passName[0] ) {
		return;
	}
	if ( !s_depthWritten ) {
		Q_strncpyz( s_depthFirstWriter, passName, sizeof( s_depthFirstWriter ) );
		s_depthWritten = qtrue;
	}
	Q_strncpyz( s_depthLastWriter, passName, sizeof( s_depthLastWriter ) );
}

void vk_depth_contract_note_reader( const char *passName )
{
	if ( !passName || !passName[0] ) {
		return;
	}
	if ( s_depthReaderCount >= 8u ) {
		return;
	}
	Q_strncpyz( s_depthReaders[s_depthReaderCount], passName, sizeof( s_depthReaders[0] ) );
	s_depthReaderCount++;
}
