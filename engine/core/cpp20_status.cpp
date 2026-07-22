/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

cpp20_status — migration report console command.
See docs/CPP20_MIGRATION.md. Counts are maintained in cpp20_migration_status.inc.
===========================================================================
*/

#include "cpp20_compat.h"

IDTECH3_EXTERN_C_BEGIN
#include "q_shared.h"
#include "qcommon.h"
IDTECH3_EXTERN_C_END

#include "cpp20_migration_status.inc"

extern "C" void Com_Cpp20AbiGuards_Touch( void );

static void Com_Cpp20Status_f( void )
{
	Com_Cpp20AbiGuards_Touch();
	Com_Printf( "cpp20_status:\n" );
	Com_Printf( "  USE_CPP20=%d EXCEPTIONS=%d RTTI=%d STRICT(build)=see CMake\n",
#if defined(USE_CPP20) && USE_CPP20
		1,
#else
		0,
#endif
#if defined(CPP20_EXCEPTIONS) && CPP20_EXCEPTIONS
		1,
#else
		0,
#endif
#if defined(CPP20_RTTI) && CPP20_RTTI
		1
#else
		0
#endif
	);
	Com_Printf( "  total_c_tracked=%d total_cxx_tracked=%d\n",
		CPP20_STATUS_TOTAL_C, CPP20_STATUS_TOTAL_CXX );
	Com_Printf( "  converted_leaves=%d blocked_high_risk=%d\n",
		CPP20_STATUS_CONVERTED, CPP20_STATUS_BLOCKED );
	Com_Printf( "  mixed_language_boundaries=%d c_abi_boundaries=%d\n",
		CPP20_STATUS_MIXED_BOUNDARIES, CPP20_STATUS_C_ABI_BOUNDARIES );
	Com_Printf( "  remaining_high_risk=%d\n", CPP20_STATUS_HIGH_RISK_REMAINING );
	Com_Printf( "  cxx_standard=C++%d\n",
#ifdef IDTECH3_CXX_STANDARD
		IDTECH3_CXX_STANDARD
#else
		20
#endif
	);
	Com_Printf( "  inventory=docs/cpp20_inventory.tsv docs=docs/CPP20_MIGRATION.md\n" );
}

extern "C" void Com_Cpp20Status_Init( void )
{
	Cmd_AddCommand( "cpp20_status", Com_Cpp20Status_f );
	Com_Printf( "cpp20_status: C++20 migration report command ready\n" );
}
