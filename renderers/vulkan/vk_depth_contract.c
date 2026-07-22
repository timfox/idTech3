/*
===========================================================================
Scene depth contract — reversed-Z depth writer/reader tracking.
Foundation Consolidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_depth_contract.h"

static char s_depthFirstWriter[48];
static char s_depthLastWriter[48];
static qboolean s_depthWritten;

void vk_depth_contract_register( void )
{
	ri.Printf( PRINT_ALL, "[VK][depth] depth contract ready (reversed-Z GREATER_OR_EQUAL)\n" );
}

void vk_depth_contract_begin_frame( void )
{
	s_depthFirstWriter[0] = '\0';
	s_depthLastWriter[0] = '\0';
	s_depthWritten = qfalse;
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
	(void)passName;
}
