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

#include "q_shared.h"
#include "qcommon.h"
#include "q_memtrack.h"
#include "q_log.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef ENABLE_MEMORY_TRACKING

// Thread safety (simple mutex simulation for now)
static qboolean memtrack_initialized = qfalse;
static qboolean memtrack_enabled = qtrue;

// Memory records (linked list)
static memrecord_t *mem_records = NULL;
static int mem_record_count = 0;

// Statistics per memory type
static memstats_t mem_stats[MEMTYPE_COUNT];

// CVars
static cvar_t *memtrack_enable;
static cvar_t *memtrack_report_leaks;
static cvar_t *memtrack_log_leaks;

// Advanced memory management cvars
static cvar_t *mem_pool_enable;
static cvar_t *mem_pool_size;
static cvar_t *mem_defrag_enable;
static cvar_t *mem_cache_enable;
static cvar_t *mem_predictive_alloc;

/*
================
Q_MemTrack_Init
================
*/
void Q_MemTrack_Init(void) {
	if (memtrack_initialized) {
		return;
	}
	
	Com_Memset(mem_stats, 0, sizeof(mem_stats));
	mem_records = NULL;
	mem_record_count = 0;
	
	// Register CVars
	memtrack_enable = Cvar_Get("memtrack_enable", "1", CVAR_ARCHIVE, "Enable memory tracking");
	Cvar_SetDescription(memtrack_enable, "Enable memory usage tracking and statistics");
	memtrack_report_leaks = Cvar_Get("memtrack_report_leaks", "0", CVAR_ARCHIVE, "Report memory leaks on shutdown");
	Cvar_SetDescription(memtrack_report_leaks, "Report memory leaks when engine shuts down");
	memtrack_log_leaks = Cvar_Get("memtrack_log_leaks", "0", CVAR_ARCHIVE, "Log memory leaks to file");
	Cvar_SetDescription(memtrack_log_leaks, "Log memory leaks to memleaks.log");

	// Advanced memory management cvars
	mem_pool_enable = Cvar_Get("mem_pool_enable", "1", CVAR_ARCHIVE, "Enable memory pool allocation");
	Cvar_SetDescription(mem_pool_enable, "Enable advanced memory pool allocation system");
	mem_pool_size = Cvar_Get("mem_pool_size", "16777216", CVAR_ARCHIVE, "Memory pool size");
	Cvar_SetDescription(mem_pool_size, "Size of memory pool in bytes (default 16MB)");
	mem_defrag_enable = Cvar_Get("mem_defrag_enable", "0", CVAR_ARCHIVE, "Enable memory defragmentation");
	Cvar_SetDescription(mem_defrag_enable, "Enable memory defragmentation (experimental, may impact performance)");
	mem_cache_enable = Cvar_Get("mem_cache_enable", "1", CVAR_ARCHIVE, "Enable allocation caching");
	Cvar_SetDescription(mem_cache_enable, "Enable memory allocation result caching for performance");
	mem_predictive_alloc = Cvar_Get("mem_predictive_alloc", "0", CVAR_ARCHIVE, "Enable predictive allocation");
	Cvar_SetDescription(mem_predictive_alloc, "Enable predictive memory allocation based on usage patterns");

	// Crash resilience cvars
	com_crash_minidump = Cvar_Get("com_crash_minidump", "1", CVAR_ARCHIVE, "Generate minidump on crash");
	Cvar_SetDescription(com_crash_minidump, "Generate minidump files for crash analysis");
	com_crash_log_buffer = Cvar_Get("com_crash_log_buffer", "4096", CVAR_ARCHIVE, "Log buffer size for crash");
	Cvar_SetDescription(com_crash_log_buffer, "Size of log ring buffer preserved on crash (KB)");
	com_crash_auto_restart = Cvar_Get("com_crash_auto_restart", "0", CVAR_ARCHIVE, "Auto restart after crash");
	Cvar_SetDescription(com_crash_auto_restart, "Automatically restart engine after fatal crash");
	com_crash_telemetry = Cvar_Get("com_crash_telemetry", "0", CVAR_ARCHIVE, "Send crash telemetry");
	Cvar_SetDescription(com_crash_telemetry, "Send anonymous crash data for debugging (requires user consent)");
	
	memtrack_initialized = qtrue;
	memtrack_enabled = qtrue;
	
	Q_LogInfo(LOG_CATEGORY_MEMORY, "Memory tracking system initialized");
}

/*
================
Q_MemTrack_Shutdown
================
*/
void Q_MemTrack_Shutdown(void) {
	if (!memtrack_initialized) {
		return;
	}
	
	if (memtrack_enabled && memtrack_report_leaks && memtrack_report_leaks->integer) {
		Q_MemTrack_ReportLeaks();
	}
	
	// Free all records (they should already be freed, but clean up anyway)
	memrecord_t *record = mem_records;
	while (record) {
		memrecord_t *next = record->next;
		free(record);
		record = next;
	}
	mem_records = NULL;
	mem_record_count = 0;
	
	memtrack_initialized = qfalse;
	
	Q_LogInfo(LOG_CATEGORY_MEMORY, "Memory tracking system shut down");
}

/*
================
Q_MemTrack_Alloc
================
*/
void *Q_MemTrack_Alloc(size_t size, memtype_t type, const char *file, int line, const char *func) {
	if (!memtrack_initialized || !memtrack_enabled) {
		return malloc(size);
	}
	
	void *ptr = malloc(size);
	if (!ptr) {
		return NULL;
	}
	
	// Create record
	memrecord_t *record = (memrecord_t *)malloc(sizeof(memrecord_t));
	if (record) {
		record->ptr = ptr;
		record->size = size;
		record->type = type;
		record->file = file;
		record->line = line;
		record->func = func;
		record->timestamp = time(NULL);
		record->next = mem_records;
		mem_records = record;
		mem_record_count++;
		
		// Update statistics
		memstats_t *stats = &mem_stats[type];
		stats->allocated += size;
		stats->current += size;
		stats->count++;
		
		if (stats->current > stats->peak) {
			stats->peak = stats->current;
		}
		
		// Valgrind integration
		Q_VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 0);
	}
	
	return ptr;
}

/*
================
Q_MemTrack_Realloc
================
*/
void *Q_MemTrack_Realloc(void *ptr, size_t size, memtype_t type, const char *file, int line, const char *func) {
	if (!memtrack_initialized || !memtrack_enabled) {
		return realloc(ptr, size);
	}
	
	// Find and remove old record
	memrecord_t *record = mem_records;
	memrecord_t *prev = NULL;
	size_t old_size = 0;
	
	while (record) {
		if (record->ptr == ptr) {
			old_size = record->size;
			if (prev) {
				prev->next = record->next;
			} else {
				mem_records = record->next;
			}
			free(record);
			mem_record_count--;
			break;
		}
		prev = record;
		record = record->next;
	}
	
	// Reallocate
	void *new_ptr = realloc(ptr, size);
	if (!new_ptr) {
		return NULL;
	}
	
	// Update statistics
	if (old_size > 0) {
		memstats_t *stats = &mem_stats[type];
		stats->freed += old_size;
		stats->current -= old_size;
		stats->free_count++;
	}
	
	// Create new record
	record = (memrecord_t *)malloc(sizeof(memrecord_t));
	if (record) {
		record->ptr = new_ptr;
		record->size = size;
		record->type = type;
		record->file = file;
		record->line = line;
		record->func = func;
		record->timestamp = time(NULL);
		record->next = mem_records;
		mem_records = record;
		mem_record_count++;
		
		// Update statistics
		memstats_t *stats = &mem_stats[type];
		stats->allocated += size;
		stats->current += size;
		stats->count++;
		
		if (stats->current > stats->peak) {
			stats->peak = stats->current;
		}
	}
	
	return new_ptr;
}

/*
================
Q_MemTrack_Free
================
*/
void Q_MemTrack_Free(void *ptr, const char *file, int line, const char *func) {
	if (!ptr) {
		return;
	}
	
	if (!memtrack_initialized || !memtrack_enabled) {
		free(ptr);
		return;
	}
	
	// Find and remove record
	memrecord_t *record = mem_records;
	memrecord_t *prev = NULL;
	
	while (record) {
		if (record->ptr == ptr) {
			// Update statistics
			memstats_t *stats = &mem_stats[record->type];
			stats->freed += record->size;
			stats->current -= record->size;
			stats->free_count++;
			
			// Remove from list
			if (prev) {
				prev->next = record->next;
			} else {
				mem_records = record->next;
			}
			free(record);
			mem_record_count--;
			
			// Valgrind integration
			Q_VALGRIND_FREELIKE_BLOCK(ptr, 0);
			
			free(ptr);
			return;
		}
		prev = record;
		record = record->next;
	}
	
	// Not found in records - double free or invalid pointer
	Q_LogWarn(LOG_CATEGORY_MEMORY, "Attempted to free untracked pointer %p at %s:%d", ptr, file, line);
	free(ptr);
}

/*
================
Q_MemTrack_GetStats
================
*/
void Q_MemTrack_GetStats(memtype_t type, memstats_t *stats) {
	if (!stats) {
		return;
	}
	
	if (type < 0 || type >= MEMTYPE_COUNT) {
		Com_Memset(stats, 0, sizeof(memstats_t));
		return;
	}
	
	*stats = mem_stats[type];
}

/*
================
Q_MemTrack_GetTotalStats
================
*/
void Q_MemTrack_GetTotalStats(memstats_t *stats) {
	if (!stats) {
		return;
	}
	
	Com_Memset(stats, 0, sizeof(memstats_t));
	
	for (int i = 0; i < MEMTYPE_COUNT; i++) {
		stats->allocated += mem_stats[i].allocated;
		stats->freed += mem_stats[i].freed;
		stats->current += mem_stats[i].current;
		if (mem_stats[i].peak > stats->peak) {
			stats->peak = mem_stats[i].peak;
		}
		stats->count += mem_stats[i].count;
		stats->free_count += mem_stats[i].free_count;
		stats->leak_count += mem_stats[i].leak_count;
	}
}

/*
================
Q_MemTrack_GetLeakCount
================
*/
int Q_MemTrack_GetLeakCount(void) {
	return mem_record_count;
}

/*
================
Q_MemTrack_ReportLeaks
================
*/
void Q_MemTrack_ReportLeaks(void) {
	if (!memtrack_initialized || !memtrack_enabled) {
		return;
	}
	
	if (mem_record_count == 0) {
		Q_LogInfo(LOG_CATEGORY_MEMORY, "No memory leaks detected");
		return;
	}
	
	Q_LogWarn(LOG_CATEGORY_MEMORY, "Memory leak report: %d potential leaks detected", mem_record_count);
	
	// Group leaks by type
	int leaks_by_type[MEMTYPE_COUNT] = {0};
	int64_t bytes_by_type[MEMTYPE_COUNT] = {0};
	
	memrecord_t *record = mem_records;
	while (record) {
		leaks_by_type[record->type]++;
		bytes_by_type[record->type] += record->size;
		record = record->next;
	}
	
	// Report summary
	const char *type_names[] = {
		"HUNK", "ZONE", "TEMP", "SOUND", "RENDERER",
		"NETWORK", "FILESYSTEM", "SCRIPT", "BOTLIB", "OTHER"
	};
	
	for (int i = 0; i < MEMTYPE_COUNT; i++) {
		if (leaks_by_type[i] > 0) {
			Q_LogWarn(LOG_CATEGORY_MEMORY, "  %s: %d leaks, %lld bytes", 
				type_names[i], leaks_by_type[i], (long long)bytes_by_type[i]);
		}
	}
	
	// Detailed report if logging enabled
	if (memtrack_log_leaks && memtrack_log_leaks->integer) {
		fileHandle_t f = FS_FOpenFileWrite("memleaks.log");
		if (f != FS_INVALID_HANDLE) {
			char buffer[1024];
			int len;
			
			len = Com_sprintf(buffer, sizeof(buffer), 
				"Memory Leak Report\n"
				"==================\n"
				"Total leaks: %d\n\n", mem_record_count);
			FS_Write(buffer, len, f);
			
			record = mem_records;
			while (record) {
				len = Com_sprintf(buffer, sizeof(buffer),
					"Leak: %p, Size: %zu bytes, Type: %s\n"
					"  Location: %s:%d in %s()\n"
					"  Timestamp: %ld\n\n",
					record->ptr, record->size, type_names[record->type],
					record->file, record->line, record->func ? record->func : "unknown",
					(long)record->timestamp);
				FS_Write(buffer, len, f);
				record = record->next;
			}
			
			FS_FCloseFile(f);
			Q_LogInfo(LOG_CATEGORY_MEMORY, "Leak report written to memleaks.log");
		}
	}
}

#else // ENABLE_MEMORY_TRACKING

// Stub implementations when tracking is disabled
void Q_MemTrack_Init(void) {}
void Q_MemTrack_Shutdown(void) {}
void *Q_MemTrack_Alloc(size_t size, memtype_t type, const char *file, int line, const char *func) {
	(void)type; (void)file; (void)line; (void)func; // Suppress unused parameter warnings
	return malloc(size);
}
void *Q_MemTrack_Realloc(void *old_ptr, size_t size, memtype_t type, const char *file, int line, const char *func) {
	(void)type; (void)file; (void)line; (void)func; // Suppress unused parameter warnings
	return realloc(old_ptr, size);
}
void Q_MemTrack_Free(void *ptr, const char *file, int line, const char *func) {
	(void)file; (void)line; (void)func; // Suppress unused parameter warnings
	free(ptr);
}
void Q_MemTrack_GetStats(memtype_t type, memstats_t *stats) {
	(void)type; // Suppress unused parameter warning
	if (stats) {
		Com_Memset(stats, 0, sizeof(memstats_t));
	}
}
void Q_MemTrack_GetTotalStats(memstats_t *stats) {
	if (stats) {
		Com_Memset(stats, 0, sizeof(memstats_t));
	}
}
int Q_MemTrack_GetLeakCount(void) { return 0; }
void Q_MemTrack_ReportLeaks(void) {}

#endif // ENABLE_MEMORY_TRACKING

