#include "tr_local.h"
#include "vk.h"
#include "vk_neural_deferred.h"
#include "vk_deferred_gbuffer.h"
#include "vk_forward_plus.h"

/* PBNDS+ integration boundary.  Model execution and weight upload are
 * intentionally not implied by this gate: the contract first makes inputs,
 * ownership, and dark-energy behavior observable. */
static cvar_t *r_neuralDeferred;
static cvar_t *r_neuralDeferredStrength;
static cvar_t *r_neuralDeferredDarkGate;
static cvar_t *r_neuralDeferredDebug;
static vkNeuralDeferredContract_t s_contract;
static qboolean s_registered;

void vk_neural_deferred_init( void )
{
	if ( s_registered ) return;
	r_neuralDeferred = ri.Cvar_Get( "r_neuralDeferred", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_neuralDeferred, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_neuralDeferred,
		"PBNDS+ convolutional neural deferred shading advisory; experimental, vid_restart." );
	r_neuralDeferredStrength = ri.Cvar_Get( "r_neuralDeferredStrength", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_neuralDeferredStrength, "0.0", "1.0", CV_FLOAT );
	r_neuralDeferredDarkGate = ri.Cvar_Get( "r_neuralDeferredDarkGate", "0.001", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_neuralDeferredDarkGate, "0.0", "1.0", CV_FLOAT );
	r_neuralDeferredDebug = ri.Cvar_Get( "r_neuralDeferredDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_neuralDeferredDebug, "0", "2", CV_INTEGER );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_contract.channelCount = 14u; /* A3 N3 spec3 rough depth + incident RGB */
	s_contract.owner = 0u;
	s_contract.outputOwner = 1u; /* compare/advisory until model validation */
	s_contract.darkEnergyGate = 1u;
	s_contract.inputConvention = 1u; /* linear HDR, encoded normal, material seam */
	ri.Cmd_AddCommand( "neural_deferred_status", vk_neural_deferred_status_f );
	s_registered = qtrue;
}

void vk_neural_deferred_shutdown( void )
{
	if ( !s_registered ) return;
	ri.Cmd_RemoveCommand( "neural_deferred_status" );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_registered = qfalse;
}

const vkNeuralDeferredContract_t *vk_neural_deferred_contract( void )
{
	return &s_contract;
}

void vk_neural_deferred_status_f( void )
{
	qboolean active = r_neuralDeferred && r_neuralDeferred->integer;
	qboolean inputs = vk.deferredGbufferAllocated &&
		vk.forward_plus.tile_buffer != VK_NULL_HANDLE &&
		vk.forward_plus.param_buffer != VK_NULL_HANDLE;
	ri.Printf( PRINT_ALL,
		"[VK][NeuralDeferred] enabled=%d owner=%s channels=%u gbuffer=%u "
		"clusters=%u strength=%.3f darkGate=%.4f output=%s state=%s debug=%d\n",
		active ? 1 : 0,
		active && inputs ? "neural_advisory" : "none",
		s_contract.channelCount,
		vk.deferredGbufferGeneration,
		vk_cluster_list_generation(),
		r_neuralDeferredStrength ? r_neuralDeferredStrength->value : 0.25f,
		r_neuralDeferredDarkGate ? r_neuralDeferredDarkGate->value : 0.001f,
		"compare_only",
		active && inputs ? "contract_only" : "inactive",
		r_neuralDeferredDebug ? r_neuralDeferredDebug->integer : 0 );
}

