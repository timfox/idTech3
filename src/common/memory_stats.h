/*
===========================================================================
Memory Usage Statistics Tracking
===========================================================================
*/

#ifndef __MEMORY_STATS_H__
#define __MEMORY_STATS_H__

#include "q_shared.h"
#include "qcommon.h"
#include "thread_platform.h"

// Memory statistics per tag
typedef struct {
	atomic_int64_t current;		// Current bytes allocated
	atomic_int64_t peak;			// Peak bytes allocated
	atomic_int64_t total_allocated;	// Total bytes ever allocated
	atomic_int64_t total_freed;		// Total bytes ever freed
	atomic_int_t allocations;		// Number of allocations
	atomic_int_t frees;				// Number of frees
	atomic_int_t blocks;				// Current number of blocks
} memtag_stats_t;

// Initialize memory statistics tracking
void MemStats_Init(void);
void MemStats_Shutdown(void);

// Update statistics (called from Z_TagMalloc/Z_Free)
void MemStats_Alloc(memtag_t tag, int size);
void MemStats_Free(memtag_t tag, int size);

// Get statistics for a specific tag
void MemStats_GetTagStats(memtag_t tag, memtag_stats_t *stats);

// Get total statistics across all tags
void MemStats_GetTotalStats(memtag_stats_t *stats);

// Console command to display memory statistics
void MemStats_Display_f(void);

// Get formatted memory info string (for HUD/debug display)
void MemStats_GetInfoString(char *buffer, int bufferSize);

#endif // __MEMORY_STATS_H__
