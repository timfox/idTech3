/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

C++20 ABI layout guards for critical shared structures.
Linux x86_64 baseline sizes — extend tables for other platforms.
See docs/CPP20_ABI_BOUNDARIES.md.
===========================================================================
*/

#include "cpp20_compat.h"

IDTECH3_EXTERN_C_BEGIN
#include "q_shared.h"
#include "qcommon.h"
IDTECH3_EXTERN_C_END

#include <cstddef>

#if defined(__linux__) && defined(__x86_64__) && !defined(Q3_VM)

static_assert( sizeof( qboolean ) == 1, "qboolean ABI size (linux amd64)" );
static_assert( sizeof( trace_t ) == 76, "trace_t ABI size (linux amd64)" );
static_assert( offsetof( trace_t, allsolid ) == 0, "trace_t.allsolid offset" );
static_assert( sizeof( usercmd_t ) == 24, "usercmd_t ABI size (linux amd64)" );
static_assert( sizeof( entityState_t ) == 208, "entityState_t ABI size (linux amd64)" );
static_assert( sizeof( playerState_t ) == 468, "playerState_t ABI size (linux amd64)" );
static_assert( sizeof( netadr_t ) == 28, "netadr_t ABI size (linux amd64)" );
static_assert( sizeof( msg_t ) == 40, "msg_t ABI size (linux amd64)" );

#endif /* linux amd64 */

/* Cross-platform relative checks (layout intent). */
static_assert( sizeof( vec3_t ) == sizeof( float ) * 3, "vec3_t is 3 floats" );
static_assert( sizeof( qboolean ) == sizeof( bool ) || sizeof( qboolean ) == sizeof( int ),
	"qboolean is bool (native) or int-sized (legacy VM path)" );

/* Force the TU into the link so asserts run at compile time for engine targets. */
extern "C" void Com_Cpp20AbiGuards_Touch( void );
void Com_Cpp20AbiGuards_Touch( void ) {}
