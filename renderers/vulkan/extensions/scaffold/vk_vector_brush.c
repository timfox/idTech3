#include "tr_local.h"
#include "vk_vector_brush.h"

/* Ciallo sidecar gate.  This owns only 2D vector overlay work; it must not
 * become an implicit material, G-buffer, or clustered-light path. */
static cvar_t *r_vectorBrush;
static cvar_t *r_vectorBrushSpacing;
static cvar_t *r_vectorBrushMaxStamps;
static cvar_t *r_vectorBrushDebug;
static vkVectorBrushContract_t s_contract;
static qboolean s_registered;

void vk_vector_brush_init( void )
{
	if ( s_registered ) return;
	r_vectorBrush = ri.Cvar_Get( "r_vectorBrush", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vectorBrush, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vectorBrush,
		"GPU vector brush overlay sidecar (Ciallo foundation); experimental, vid_restart." );
	r_vectorBrushSpacing = ri.Cvar_Get( "r_vectorBrushSpacing", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vectorBrushSpacing, "0.125", "32.0", CV_FLOAT );
	r_vectorBrushMaxStamps = ri.Cvar_Get( "r_vectorBrushMaxStamps", "256", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vectorBrushMaxStamps, "8", "4096", CV_INTEGER );
	r_vectorBrushDebug = ri.Cvar_Get( "r_vectorBrushDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vectorBrushDebug, "0", "2", CV_INTEGER );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_contract.owner = 0u;
	s_contract.target = 1u;
	s_contract.airbrushContinuous = 1u;
	ri.Cmd_AddCommand( "vector_brush_status", vk_vector_brush_status_f );
	s_registered = qtrue;
}

void vk_vector_brush_shutdown( void )
{
	if ( !s_registered ) return;
	ri.Cmd_RemoveCommand( "vector_brush_status" );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_registered = qfalse;
}

const vkVectorBrushContract_t *vk_vector_brush_contract( void )
{
	return &s_contract;
}

void vk_vector_brush_status_f( void )
{
	qboolean active = r_vectorBrush && r_vectorBrush->integer;
	ri.Printf( PRINT_ALL,
		"[VK][VectorBrush] enabled=%d owner=%s target=overlay spacing=%.3f "
		"maxStamps=%d airbrush=continuous fillMarkers=%u state=%s debug=%d\n",
		active ? 1 : 0,
		active ? "vector_overlay" : "none",
		r_vectorBrushSpacing ? r_vectorBrushSpacing->value : 1.0f,
		r_vectorBrushMaxStamps ? r_vectorBrushMaxStamps->integer : 256,
		s_contract.fillMarkers,
		active ? "resample_contract_only" : "inactive",
		r_vectorBrushDebug ? r_vectorBrushDebug->integer : 0 );
}

