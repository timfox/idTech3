/*
===========================================================================
Lightweight profiler shim (Tracy-compatible where available)
===========================================================================
*/

#ifndef __PROFILER_H__
#define __PROFILER_H__

#include "q_shared.h"

// Detect TracyC.h availability when USE_TRACY is enabled
#if defined(USE_TRACY) && defined(__has_include)
#	if __has_include("TracyC.h")
#		include "TracyC.h"
#		define PROFILER_TRACY 1
#	else
#		define PROFILER_TRACY 0
#	endif
#elif defined(USE_TRACY)
#	define PROFILER_TRACY 0
#else
#	define PROFILER_TRACY 0
#endif

#if PROFILER_TRACY
#	define PROF_ENABLED 1
#	define PROF_THREAD_NAME(name) TracyCSetThreadName(name)
#	define PROF_FRAME_MARK() TracyCFrameMark
#	define PROF_ZONE_BEGIN(ctx, name) TracyCZoneN(ctx, name, 1)
#	define PROF_ZONE_BEGIN_COLOR(ctx, name, color) TracyCZoneNC(ctx, name, color, 1)
#	define PROF_ZONE_END(ctx) TracyCZoneEnd(ctx)
#else
	typedef void *TracyCZoneCtx;
#	define PROF_ENABLED 0
#	define PROF_THREAD_NAME(name) ((void)(name))
#	define PROF_FRAME_MARK() ((void)0)
#	define PROF_ZONE_BEGIN(ctx, name) ((void)(ctx))
#	define PROF_ZONE_BEGIN_COLOR(ctx, name, color) ((void)(ctx))
#	define PROF_ZONE_END(ctx) ((void)(ctx))
#endif

#endif // __PROFILER_H__

