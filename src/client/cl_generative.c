/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_generative.h"

#if defined( USE_TRELLIS ) || defined( USE_GENETIC_GAN ) || defined( USE_FLUX )

#ifdef USE_TRELLIS
#include "cl_trellis.h"
#endif
#ifdef USE_FLUX
#include "cl_flux.h"
#endif
#ifdef USE_GENETIC_GAN
#include "cl_genetic_gan.h"
#endif

#if defined( USE_FLUX ) && defined( USE_TRELLIS )
static void CL_Generative_MaybeChainTrellisFromFlux( void )
{
	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		CL_Flux_ClearTrellisChain();
		return;
	}
	if ( !cl_trellis_chain || !cl_trellis_chain->integer ) {
		CL_Flux_ClearTrellisChain();
		return;
	}
	if ( !CL_Flux_IsTrellisChainArmed() ) {
		return;
	}
	if ( CL_Flux_IsRunning() ) {
		return;
	}
	CL_Flux_ClearTrellisChain();
	if ( CL_Flux_GetJobStatus() != CL_FLUX_JOB_COMPLETED ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: FLUX chain aborted (FLUX did not complete successfully)\n" );
		return;
	}
	{
		const char *out = CL_Flux_GetOutputPath();
		if ( !out || !out[0] ) {
			Com_Printf( S_COLOR_YELLOW "TRELLIS: FLUX chain aborted (FLUX did not complete successfully)\n" );
			return;
		}
		Com_Printf( "TRELLIS: chaining from FLUX output %s\n", out );
		CL_Trellis_StartJob( out, NULL );
	}
}
#endif

void CL_GenerativeFrame( void )
{
#ifdef USE_TRELLIS
	CL_Trellis_Frame();
#endif
#if defined( USE_FLUX )
	CL_Flux_Frame();
#endif
#if defined( USE_FLUX ) && defined( USE_TRELLIS )
	CL_Generative_MaybeChainTrellisFromFlux();
#endif
#ifdef USE_GENETIC_GAN
	CL_GeneticGan_Frame();
#endif
}

#endif
