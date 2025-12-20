/*
===========================================================================
Memory Usage Statistics Tracking
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "memory_stats.h"

static qboolean memstats_initialized = qfalse;
static memtag_stats_t memtag_stats[TAG_COUNT];
static cvar_t *memstats_enable;

/*
================
MemStats_Init
================
*/
void MemStats_Init(void) {
	if (memstats_initialized) {
		return;
	}

	Com_Memset(memtag_stats, 0, sizeof(memtag_stats));
	
	memstats_enable = Cvar_Get("memstats_enable", "1", CVAR_ARCHIVE);
	
	Cmd_AddCommand("memstats", MemStats_Display_f);
	
	memstats_initialized = qtrue;
}

/*
================
MemStats_Shutdown
================
*/
void MemStats_Shutdown(void) {
	if (!memstats_initialized) {
		return;
	}
	
	Cmd_RemoveCommand("memstats");
	memstats_initialized = qfalse;
}

/*
================
MemStats_Alloc
================
*/
void MemStats_Alloc(memtag_t tag, int size) {
	if (!memstats_initialized || !memstats_enable || !memstats_enable->integer) {
		return;
	}
	
	if (tag >= TAG_COUNT || tag == TAG_FREE) {
		return;
	}
	
	memtag_stats_t *stats = &memtag_stats[tag];
	stats->current += size;
	stats->total_allocated += size;
	stats->allocations++;
	stats->blocks++;
	
	if (stats->current > stats->peak) {
		stats->peak = stats->current;
	}
}

/*
================
MemStats_Free
================
*/
void MemStats_Free(memtag_t tag, int size) {
	if (!memstats_initialized || !memstats_enable || !memstats_enable->integer) {
		return;
	}
	
	if (tag >= TAG_COUNT || tag == TAG_FREE) {
		return;
	}
	
	memtag_stats_t *stats = &memtag_stats[tag];
	stats->current -= size;
	stats->total_freed += size;
	stats->frees++;
	if (stats->blocks > 0) {
		stats->blocks--;
	}
}

/*
================
MemStats_GetTagStats
================
*/
void MemStats_GetTagStats(memtag_t tag, memtag_stats_t *stats) {
	if (!stats || tag >= TAG_COUNT) {
		return;
	}
	
	Com_Memcpy(stats, &memtag_stats[tag], sizeof(memtag_stats_t));
}

/*
================
MemStats_GetTotalStats
================
*/
void MemStats_GetTotalStats(memtag_stats_t *stats) {
	if (!stats) {
		return;
	}
	
	Com_Memset(stats, 0, sizeof(memtag_stats_t));
	
	for (int i = 0; i < TAG_COUNT; i++) {
		if (i == TAG_FREE) {
			continue;
		}
		stats->current += memtag_stats[i].current;
		stats->peak += memtag_stats[i].peak;
		stats->total_allocated += memtag_stats[i].total_allocated;
		stats->total_freed += memtag_stats[i].total_freed;
		stats->allocations += memtag_stats[i].allocations;
		stats->frees += memtag_stats[i].frees;
		stats->blocks += memtag_stats[i].blocks;
	}
}

/*
================
MemStats_Display_f
================
*/
void MemStats_Display_f(void) {
	if (!memstats_initialized) {
		Com_Printf("Memory statistics not initialized\n");
		return;
	}
	
	// Access tagName from common.c (static, so we'll use our own list)
	static const char *tagName[TAG_COUNT] = {
		"FREE", "GENERAL", "PACK", "SEARCH-PATH", "SEARCH-PACK",
		"SEARCH-DIR", "BOTLIB", "RENDERER", "CLIENTS", "SMALL", "STATIC"
	};
	memtag_stats_t total;
	MemStats_GetTotalStats(&total);
	
	Com_Printf("\n========== Memory Usage Statistics ==========\n\n");
	Com_Printf("Total Memory:\n");
	Com_Printf("  Current:  %10lld bytes (%6.2f MB)\n", 
		(long long)total.current, (float)total.current / (1024.0f * 1024.0f));
	Com_Printf("  Peak:     %10lld bytes (%6.2f MB)\n", 
		(long long)total.peak, (float)total.peak / (1024.0f * 1024.0f));
	Com_Printf("  Allocated: %10lld bytes (%6.2f MB)\n", 
		(long long)total.total_allocated, (float)total.total_allocated / (1024.0f * 1024.0f));
	Com_Printf("  Freed:    %10lld bytes (%6.2f MB)\n", 
		(long long)total.total_freed, (float)total.total_freed / (1024.0f * 1024.0f));
	Com_Printf("  Allocations: %d\n", total.allocations);
	Com_Printf("  Frees:      %d\n", total.frees);
	Com_Printf("  Blocks:     %d\n", total.blocks);
	Com_Printf("\n");
	
	Com_Printf("Memory by Tag:\n");
	Com_Printf("%-16s %12s %12s %10s %10s %8s\n", 
		"Tag", "Current", "Peak", "Allocs", "Frees", "Blocks");
	Com_Printf("%-16s %12s %12s %10s %10s %8s\n", 
		"---", "-------", "----", "------", "-----", "------");
	
	for (int i = 0; i < TAG_COUNT; i++) {
		if (i == TAG_FREE) {
			continue;
		}
		
		memtag_stats_t *stats = &memtag_stats[i];
		if (stats->current == 0 && stats->peak == 0 && stats->allocations == 0) {
			continue;
		}
		
		const char *name = (i < TAG_COUNT) ? tagName[i] : "UNKNOWN";
		Com_Printf("%-16s %10lld KB %10lld KB %10d %10d %8d\n",
			name,
			(long long)(stats->current / 1024),
			(long long)(stats->peak / 1024),
			stats->allocations,
			stats->frees,
			stats->blocks);
	}
	
	Com_Printf("\n=============================================\n");
}

/*
================
MemStats_GetInfoString
================
*/
void MemStats_GetInfoString(char *buffer, int bufferSize) {
	if (!buffer || bufferSize <= 0) {
		return;
	}
	
	memtag_stats_t total;
	MemStats_GetTotalStats(&total);
	
	Com_sprintf(buffer, bufferSize,
		"Memory: %.2f MB current, %.2f MB peak, %d blocks",
		(float)total.current / (1024.0f * 1024.0f),
		(float)total.peak / (1024.0f * 1024.0f),
		total.blocks);
}
