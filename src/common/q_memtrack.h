/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef __Q_MEMTRACK_H__
#define __Q_MEMTRACK_H__

#include "q_shared.h"

// Memory allocation types
typedef enum {
	MEMTYPE_HUNK = 0,
	MEMTYPE_ZONE,
	MEMTYPE_TEMP,
	MEMTYPE_SOUND,
	MEMTYPE_RENDERER,
	MEMTYPE_NETWORK,
	MEMTYPE_FILESYSTEM,
	MEMTYPE_SCRIPT,
	MEMTYPE_BOTLIB,
	MEMTYPE_OTHER,
	MEMTYPE_COUNT
} memtype_t;

// Memory statistics structure
typedef struct {
	int64_t allocated;		// Total bytes allocated
	int64_t freed;			// Total bytes freed
	int64_t current;		// Current bytes in use
	int64_t peak;			// Peak bytes in use
	int64_t count;			// Number of allocations
	int64_t free_count;		// Number of frees
	int64_t leak_count;		// Number of leaks detected
} memstats_t;

// Memory allocation record (for leak detection)
typedef struct memrecord_s {
	void *ptr;
	size_t size;
	memtype_t type;
	const char *file;
	int line;
	const char *func;
	time_t timestamp;
	struct memrecord_s *next;
} memrecord_t;

// Initialize memory tracking system
void Q_MemTrack_Init(void);
void Q_MemTrack_Shutdown(void);

// Track allocation
void *Q_MemTrack_Alloc(size_t size, memtype_t type, const char *file, int line, const char *func);
void *Q_MemTrack_Realloc(void *ptr, size_t size, memtype_t type, const char *file, int line, const char *func);
void Q_MemTrack_Free(void *ptr, const char *file, int line, const char *func);

// Get statistics
void Q_MemTrack_GetStats(memtype_t type, memstats_t *stats);
void Q_MemTrack_GetTotalStats(memstats_t *stats);

// Leak detection
void Q_MemTrack_ReportLeaks(void);
int Q_MemTrack_GetLeakCount(void);

// Convenience macros (only use these, not the functions directly)
#ifdef ENABLE_MEMORY_TRACKING
#define Q_MemTrack_Malloc(size, type) Q_MemTrack_Alloc(size, type, __FILE__, __LINE__, __FUNCTION__)
#define Q_MemTrack_ReallocMacro(ptr, size, type) Q_MemTrack_Realloc(ptr, size, type, __FILE__, __LINE__, __FUNCTION__)
#define Q_MemTrack_FreeMacro(ptr) Q_MemTrack_Free(ptr, __FILE__, __LINE__, __FUNCTION__)
#else
// No-op macros when tracking is disabled
#define Q_MemTrack_Malloc(size, type) malloc(size)
#define Q_MemTrack_ReallocMacro(ptr, size, type) realloc(ptr, size)
#define Q_MemTrack_FreeMacro(ptr) free(ptr)
#endif

// Valgrind integration
#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#define Q_VALGRIND_MALLOCLIKE_BLOCK(ptr, size, rzB, is_zeroed) VALGRIND_MALLOCLIKE_BLOCK(ptr, size, rzB, is_zeroed)
#define Q_VALGRIND_FREELIKE_BLOCK(ptr, rzB) VALGRIND_FREELIKE_BLOCK(ptr, rzB)
#define Q_VALGRIND_MAKE_MEM_DEFINED(ptr, size) VALGRIND_MAKE_MEM_DEFINED(ptr, size)
#define Q_VALGRIND_MAKE_MEM_UNDEFINED(ptr, size) VALGRIND_MAKE_MEM_UNDEFINED(ptr, size)
#else
#define Q_VALGRIND_MALLOCLIKE_BLOCK(ptr, size, rzB, is_zeroed) ((void)0)
#define Q_VALGRIND_FREELIKE_BLOCK(ptr, rzB) ((void)0)
#define Q_VALGRIND_MAKE_MEM_DEFINED(ptr, size) ((void)0)
#define Q_VALGRIND_MAKE_MEM_UNDEFINED(ptr, size) ((void)0)
#endif

// Dr. Memory integration (Windows)
#ifdef _WIN32
#ifdef HAVE_DRMEMORY
#include <drmemory.h>
#define Q_DRMEMORY_MALLOC(ptr, size) DrMemoryMalloc(ptr, size)
#define Q_DRMEMORY_FREE(ptr) DrMemoryFree(ptr)
#else
#define Q_DRMEMORY_MALLOC(ptr, size) ((void)0)
#define Q_DRMEMORY_FREE(ptr) ((void)0)
#endif
#endif

#endif // __Q_MEMTRACK_H__

