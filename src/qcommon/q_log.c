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
#include "q_log.h"
#include <time.h>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#include <syslog.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <io.h>
#endif

#define MAX_LOG_MESSAGE		8192
#define MAX_LOG_FILENAME		256
#define DEFAULT_ROTATION_SIZE_MB	100
#define DEFAULT_ROTATION_TIME_HOURS	24
#define MAX_LOG_FILES			10
#define MAX_DEFERRED_MESSAGES	256	// Maximum number of deferred log messages

// Deferred log message structure
typedef struct {
	char message[MAX_LOG_MESSAGE];
	int len;
	log_level_t level;
	log_category_t category;
} deferred_log_message_t;

// Logging state
static struct {
	qboolean initialized;
	log_level_t global_level;
	log_format_t format;
	int output_flags;
	
	// Category state
	qboolean category_enabled[LOG_CATEGORY_COUNT];
	log_level_t category_level[LOG_CATEGORY_COUNT];
	
	// File output
	char filename[MAX_LOG_FILENAME];
	fileHandle_t file_handle;
	int rotation_size_mb;
	int rotation_time_hours;
	time_t rotation_time;
	int current_file_size;
	
	// Deferred logging queue (for when filesystem is not ready)
	deferred_log_message_t deferred_queue[MAX_DEFERRED_MESSAGES];
	int deferred_count;
	qboolean defer_logging;	// Set to true when filesystem is restarting
	
	// Thread safety (simple for now)
	qboolean in_log;
} log_state;

// CVars
static cvar_t *log_enable;
static cvar_t *log_level;
static cvar_t *log_format;
static cvar_t *log_output;
static cvar_t *log_file;
static cvar_t *log_rotation_size;
static cvar_t *log_rotation_time;
static cvar_t *log_category_filter;

// Category names
static const char *category_names[LOG_CATEGORY_COUNT] = {
	"general",
	"client",
	"server",
	"renderer",
	"network",
	"filesystem",
	"sound",
	"input",
	"physics",
	"ai",
	"script",
	"memory"
};

// Level names
static const char *level_names[LOG_LEVEL_COUNT] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL"
};

// Syslog level mapping
#ifndef _WIN32
static int syslog_levels[LOG_LEVEL_COUNT] = {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERR,
	LOG_CRIT
};
#endif

// Forward declarations
static void Q_Log_WriteToFile(const char *message, int len);
static void Q_Log_WriteToConsole(const char *message, int len);
static void Q_Log_WriteToSyslog(log_level_t level, log_category_t category, const char *message);
static void Q_Log_CheckRotation(void);
static void Q_Log_RotateFile(void);
static qboolean Q_Log_ShouldLog(log_level_t level, log_category_t category);
static void Q_Log_FormatText(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *message, char *buffer, int buffer_size);
static void Q_Log_FormatJSON(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *message, char *buffer, int buffer_size);

/*
================
Q_Log_GetCategoryName
================
*/
const char *Q_Log_GetCategoryName(log_category_t category) {
	if (category >= 0 && category < LOG_CATEGORY_COUNT) {
		return category_names[category];
	}
	return "unknown";
}

/*
================
Q_Log_GetLevelName
================
*/
const char *Q_Log_GetLevelName(log_level_t level) {
	if (level >= 0 && level < LOG_LEVEL_COUNT) {
		return level_names[level];
	}
	return "UNKNOWN";
}

/*
================
Q_Log_CheckRotation
================
*/
static void Q_Log_CheckRotation(void) {
	if (!log_state.file_handle || log_state.file_handle == FS_INVALID_HANDLE) {
		return;
	}
	
	// Check size-based rotation
	if (log_state.rotation_size_mb > 0) {
		int max_size = log_state.rotation_size_mb * 1024 * 1024;
		if (log_state.current_file_size >= max_size) {
			Q_Log_RotateFile();
			return;
		}
	}
	
	// Check time-based rotation
	if (log_state.rotation_time_hours > 0) {
		time_t now = time(NULL);
		if (now >= log_state.rotation_time) {
			Q_Log_RotateFile();
			log_state.rotation_time = now + (log_state.rotation_time_hours * 3600);
		}
	}
}

/*
================
Q_Log_RotateFile
================
*/
static void Q_Log_RotateFile(void) {
	if (!log_state.file_handle || log_state.file_handle == FS_INVALID_HANDLE) {
		return;
	}
	
	FS_FCloseFile(log_state.file_handle);
	log_state.file_handle = FS_INVALID_HANDLE;
	log_state.current_file_size = 0;
	
	// Rotate existing files (console.log -> console.log.1, etc.)
	char old_name[MAX_LOG_FILENAME];
	char new_name[MAX_LOG_FILENAME];
	
	for (int i = MAX_LOG_FILES - 1; i > 0; i--) {
		if (i == 1) {
			Com_sprintf(old_name, sizeof(old_name), "%s", log_state.filename);
		} else {
			Com_sprintf(old_name, sizeof(old_name), "%s.%d", log_state.filename, i - 1);
		}
		Com_sprintf(new_name, sizeof(new_name), "%s.%d", log_state.filename, i);
		
		// Try to rename (may not exist, that's ok)
		// Delete old rotated file if it exists
		if (i > 1) {
			FS_HomeRemove(new_name);
		}
		// Rename current file to rotated name
		if (i == 1) {
			FS_SV_Rename(old_name, new_name);
		} else if (FS_SV_FOpenFileRead(old_name, NULL) >= 0) {
			// File exists, rename it
			FS_SV_Rename(old_name, new_name);
		}
	}
	
	// Open new file
	log_state.file_handle = FS_FOpenFileWrite(log_state.filename);
	if (log_state.file_handle == FS_INVALID_HANDLE) {
		log_state.file_handle = FS_FOpenFileAppend(log_state.filename);
	}
	
	if (log_state.file_handle != FS_INVALID_HANDLE) {
		time_t now = time(NULL);
		log_state.rotation_time = now + (log_state.rotation_time_hours * 3600);
	}
}

/*
================
Q_Log_ShouldLog
================
*/
static qboolean Q_Log_ShouldLog(log_level_t level, log_category_t category) {
	if (!log_state.initialized) {
		return qfalse;
	}
	
	if (category < 0 || category >= LOG_CATEGORY_COUNT) {
		return qfalse;
	}
	
	// Check if category is enabled
	if (!log_state.category_enabled[category]) {
		return qfalse;
	}
	
	// Check level threshold
	log_level_t threshold = log_state.category_level[category];
	if (level < threshold) {
		return qfalse;
	}
	
	return qtrue;
}

/*
================
Q_Log_FormatText
================
*/
static void Q_Log_FormatText(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *message, char *buffer, int buffer_size) {
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char timestamp[64];
	
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
	
	// Extract just the filename from the path
	const char *filename = strrchr(file, '/');
	if (!filename) {
		filename = strrchr(file, '\\');
	}
	if (filename) {
		filename++;
	} else {
		filename = file;
	}
	
	Com_sprintf(buffer, buffer_size, "[%s] [%s] [%s] %s:%d %s() - %s\n",
		timestamp,
		Q_Log_GetLevelName(level),
		Q_Log_GetCategoryName(category),
		filename,
		line,
		func ? func : "unknown",
		message);
}

/*
================
Q_Log_FormatJSON
================
*/
static void Q_Log_FormatJSON(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *message, char *buffer, int buffer_size) {
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char timestamp[64];
	
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", tm_info);
	
	// Extract just the filename from the path
	const char *filename = strrchr(file, '/');
	if (!filename) {
		filename = strrchr(file, '\\');
	}
	if (filename) {
		filename++;
	} else {
		filename = file;
	}
	
	// Escape JSON special characters in message
	char escaped_message[MAX_LOG_MESSAGE];
	int j = 0;
	for (int i = 0; message[i] != '\0' && j < MAX_LOG_MESSAGE - 1; i++) {
		switch (message[i]) {
			case '"': escaped_message[j++] = '\\'; escaped_message[j++] = '"'; break;
			case '\\': escaped_message[j++] = '\\'; escaped_message[j++] = '\\'; break;
			case '\n': escaped_message[j++] = '\\'; escaped_message[j++] = 'n'; break;
			case '\r': escaped_message[j++] = '\\'; escaped_message[j++] = 'r'; break;
			case '\t': escaped_message[j++] = '\\'; escaped_message[j++] = 't'; break;
			default: escaped_message[j++] = message[i]; break;
		}
	}
	escaped_message[j] = '\0';
	
	Com_sprintf(buffer, buffer_size,
		"{\"timestamp\":\"%s\",\"level\":\"%s\",\"category\":\"%s\",\"file\":\"%s\",\"line\":%d,\"function\":\"%s\",\"message\":\"%s\"}\n",
		timestamp,
		Q_Log_GetLevelName(level),
		Q_Log_GetCategoryName(category),
		filename,
		line,
		func ? func : "unknown",
		escaped_message);
}

/*
================
Q_Log_WriteToFile
================
*/
/*
================
Q_Log_FlushDeferred
Flush all deferred log messages when filesystem becomes ready
================
*/
static void Q_Log_FlushDeferred(void) {
	if (log_state.deferred_count == 0) {
		return;
	}
	
	// Write all deferred messages
	for (int i = 0; i < log_state.deferred_count; i++) {
		deferred_log_message_t *msg = &log_state.deferred_queue[i];
		if (log_state.output_flags & LOG_OUTPUT_FILE) {
			Q_Log_WriteToFile(msg->message, msg->len);
		}
		if (log_state.output_flags & LOG_OUTPUT_CONSOLE) {
			Q_Log_WriteToConsole(msg->message, msg->len);
		}
	}
	
	// Clear the queue
	log_state.deferred_count = 0;
	log_state.defer_logging = qfalse;
}

/*
================
Q_Log_DeferMessage
Queue a message for later writing when filesystem is ready
================
*/
static void Q_Log_DeferMessage(const char *message, int len, log_level_t level, log_category_t category) {
	if (log_state.deferred_count >= MAX_DEFERRED_MESSAGES) {
		// Queue is full - drop oldest message (FIFO)
		// Shift all messages left by one
		for (int i = 0; i < log_state.deferred_count - 1; i++) {
			log_state.deferred_queue[i] = log_state.deferred_queue[i + 1];
		}
		log_state.deferred_count--;
	}
	
	// Add new message to end of queue
	deferred_log_message_t *msg = &log_state.deferred_queue[log_state.deferred_count];
	Q_strncpyz(msg->message, message, sizeof(msg->message));
	msg->len = len < sizeof(msg->message) ? len : sizeof(msg->message) - 1;
	msg->level = level;
	msg->category = category;
	log_state.deferred_count++;
	log_state.defer_logging = qtrue;
}

static void Q_Log_WriteToFile(const char *message, int len) {
	if (!(log_state.output_flags & LOG_OUTPUT_FILE)) {
		return;
	}
	
	// CRITICAL: Don't try to use filesystem if it's not initialized yet (e.g., during FS_Startup)
	// This prevents recursive errors when Com_Printf tries to log during filesystem initialization
	// FS_Initialized() checks fs_searchpaths != NULL, which is NULL during FS_Startup
	// Also check FS_StartupInProgress() to avoid filesystem calls during FS_Restart
	// This check MUST happen before ANY filesystem operations
	if (!FS_Initialized() || FS_StartupInProgress()) {
		// Filesystem not ready - this should have been caught earlier, but be safe
		return;
	}
	
	// If we were deferring messages and filesystem is now ready, flush the queue
	// But only if filesystem is still ready (double-check)
	if (log_state.defer_logging && FS_Initialized() && !FS_StartupInProgress()) {
		Q_Log_FlushDeferred();
	}
	
	if (!log_state.file_handle || log_state.file_handle == FS_INVALID_HANDLE) {
		// Try to open file
		// CRITICAL: Double-check filesystem is ready before attempting to open file
		// FS_FOpenFileWrite/FS_FOpenFileAppend will call FS_CheckInitialized() if fs_startupInProgress is false
		// So we must ensure fs_searchpaths is set (FS_Initialized() returns true) before calling them
		if (log_state.filename[0] != '\0' && FS_Initialized() && !FS_StartupInProgress()) {
			log_state.file_handle = FS_FOpenFileWrite(log_state.filename);
			if (log_state.file_handle == FS_INVALID_HANDLE) {
				log_state.file_handle = FS_FOpenFileAppend(log_state.filename);
			}
			
			if (log_state.file_handle != FS_INVALID_HANDLE) {
				time_t now = time(NULL);
				log_state.rotation_time = now + (log_state.rotation_time_hours * 3600);
				log_state.current_file_size = 0;
			}
		}
	}
	
	if (log_state.file_handle != FS_INVALID_HANDLE) {
		Q_Log_CheckRotation();
		FS_Write(message, len, log_state.file_handle);
		log_state.current_file_size += len;
		
		// Flush if needed
		if (log_file && log_file->integer & 1) {
			FS_ForceFlush(log_state.file_handle);
		}
	}
}

/*
================
Q_Log_WriteToConsole
================
*/
static void Q_Log_WriteToConsole(const char *message, int len) {
	(void)len; // Unused, kept for API consistency
	if (!(log_state.output_flags & LOG_OUTPUT_CONSOLE)) {
		return;
	}
	
	// Write to system console
	Sys_Print(message);
	
	// Note: Client console printing is handled by Com_Printf compatibility layer
	// which calls CL_ConsolePrint when appropriate
}

/*
================
Q_Log_WriteToSyslog
================
*/
static void Q_Log_WriteToSyslog(log_level_t level, log_category_t category, const char *message) {
#ifndef _WIN32
	if (!(log_state.output_flags & LOG_OUTPUT_SYSLOG)) {
		return;
	}
	
	if (level >= 0 && level < LOG_LEVEL_COUNT) {
		syslog(syslog_levels[level], "[%s] %s", Q_Log_GetCategoryName(category), message);
	}
#endif
}

/*
================
Q_Log
================
*/
void QDECL Q_Log(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *fmt, ...) {
	va_list argptr;
	char message[MAX_LOG_MESSAGE];
	char formatted[MAX_LOG_MESSAGE];
	int len;
	
	if (log_state.in_log) {
		// Prevent recursion
		return;
	}
	
	if (!Q_Log_ShouldLog(level, category)) {
		return;
	}
	
	log_state.in_log = qtrue;
	
	// Format the message
	va_start(argptr, fmt);
	len = Q_vsnprintf(message, sizeof(message), fmt, argptr);
	va_end(argptr);
	
	if (len >= sizeof(message)) {
		len = sizeof(message) - 1;
		message[len] = '\0';
	}
	
	// Format according to output format
	if (log_state.format == LOG_FORMAT_JSON) {
		Q_Log_FormatJSON(level, category, file, line, func, message, formatted, sizeof(formatted));
	} else {
		Q_Log_FormatText(level, category, file, line, func, message, formatted, sizeof(formatted));
	}
	
	len = strlen(formatted);

	// ALWAYS check filesystem state FIRST before any file operations
	// This prevents recursive errors when filesystem is being restarted
	// CRITICAL: Check both FS_Initialized() AND FS_StartupInProgress()
	// FS_Initialized() checks fs_searchpaths != NULL, which is NULL during FS_Startup
	// FS_StartupInProgress() checks fs_startupInProgress flag
	// We need BOTH to be true for filesystem to be ready
	qboolean fs_ready = FS_Initialized() && !FS_StartupInProgress();
	
	// If filesystem is not ready, ALWAYS defer file logging, even if we think it's ready
	// This is a safety check to prevent any filesystem calls during startup/restart
	if ((log_state.output_flags & LOG_OUTPUT_FILE) && !fs_ready) {
		// Defer file logging - queue the message for later
		Q_Log_DeferMessage(formatted, len, level, category);
		// Still write to console and syslog immediately (they don't use filesystem)
		if (log_state.output_flags & LOG_OUTPUT_CONSOLE) {
			Q_Log_WriteToConsole(formatted, len);
		}
		if (log_state.output_flags & LOG_OUTPUT_SYSLOG) {
			Q_Log_WriteToSyslog(level, category, message);
		}
	} else {
		// Filesystem is ready - write normally
		// If we have deferred messages, flush them first (but only if filesystem is ready)
		if (log_state.defer_logging && fs_ready) {
			Q_Log_FlushDeferred();
		}
		
		// Write to outputs
		if (log_state.output_flags & LOG_OUTPUT_CONSOLE) {
			Q_Log_WriteToConsole(formatted, len);
		}
		
		if (log_state.output_flags & LOG_OUTPUT_FILE) {
			// Triple-check filesystem is still ready before writing
			// Re-check here because filesystem state might have changed between checks
			qboolean fs_still_ready = FS_Initialized() && !FS_StartupInProgress();
			if (fs_still_ready && fs_ready) {
				Q_Log_WriteToFile(formatted, len);
			} else {
				// Filesystem became unavailable - defer instead
				// This can happen if FS_Restart is called between the first check and now
				Q_Log_DeferMessage(formatted, len, level, category);
			}
		}
		
		if (log_state.output_flags & LOG_OUTPUT_SYSLOG) {
			Q_Log_WriteToSyslog(level, category, message);
		}
	}
	
	log_state.in_log = qfalse;
}

/*
================
Q_Log_Init
================
*/
void Q_Log_Init(void) {
	if (log_state.initialized) {
		return;
	}
	
	Com_Memset(&log_state, 0, sizeof(log_state));
	log_state.deferred_count = 0;
	log_state.defer_logging = qfalse;
	
	// Register CVars
	log_enable = Cvar_Get("log_enable", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(log_enable, "Enable structured logging (0=disabled, 1=enabled)");
	log_level = Cvar_Get("log_level", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(log_level, "Global log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL)");
	log_format = Cvar_Get("log_format", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(log_format, "Log format (0=text, 1=JSON)");
	log_output = Cvar_Get("log_output", "3", CVAR_ARCHIVE);
	Cvar_SetDescription(log_output, "Log output destinations (1=console, 2=file, 4=syslog, combine with +)");
	log_file = Cvar_Get("log_file", "console.log", CVAR_ARCHIVE);
	Cvar_SetDescription(log_file, "Log file name");
	log_rotation_size = Cvar_Get("log_rotation_size", va("%d", DEFAULT_ROTATION_SIZE_MB), CVAR_ARCHIVE);
	Cvar_SetDescription(log_rotation_size, "Log rotation size in MB (0=disabled)");
	log_rotation_time = Cvar_Get("log_rotation_time", va("%d", DEFAULT_ROTATION_TIME_HOURS), CVAR_ARCHIVE);
	Cvar_SetDescription(log_rotation_time, "Log rotation time in hours (0=disabled)");
	log_category_filter = Cvar_Get("log_category_filter", "", CVAR_ARCHIVE);
	Cvar_SetDescription(log_category_filter, "Category filter (comma-separated list, prefix with - to disable)");
	
	// Initialize defaults
	log_state.global_level = LOG_LEVEL_INFO;
	log_state.format = LOG_FORMAT_TEXT;
	// Default to console‑only; file logging can be re‑enabled explicitly via cvar.
	// The file backend is more complex (FS_Restart, homepath, rotation, etc.) and
	// we want the engine to be rock‑solid even if that path misbehaves.
	log_state.output_flags = LOG_OUTPUT_CONSOLE;
	Q_strncpyz(log_state.filename, "console.log", sizeof(log_state.filename));
	log_state.rotation_size_mb = DEFAULT_ROTATION_SIZE_MB;
	log_state.rotation_time_hours = DEFAULT_ROTATION_TIME_HOURS;
	
	// Enable all categories by default
	for (int i = 0; i < LOG_CATEGORY_COUNT; i++) {
		log_state.category_enabled[i] = qtrue;
		log_state.category_level[i] = LOG_LEVEL_DEBUG;
	}
	
	// Apply CVar values
	if (log_enable && log_enable->integer) {
		if (log_level) {
			int level = log_level->integer;
			if (level >= 0 && level < LOG_LEVEL_COUNT) {
				log_state.global_level = (log_level_t)level;
			}
		}
		
		if (log_format) {
			log_state.format = (log_format_t)log_format->integer;
		}
		
		if (log_output) {
			log_state.output_flags = log_output->integer;
		}
		
		if (log_file && log_file->string[0]) {
			Q_strncpyz(log_state.filename, log_file->string, sizeof(log_state.filename));
		}
		
		if (log_rotation_size) {
			log_state.rotation_size_mb = log_rotation_size->integer;
		}
		
		if (log_rotation_time) {
			log_state.rotation_time_hours = log_rotation_time->integer;
		}
		
		// Parse category filter
		if (log_category_filter && log_category_filter->string[0]) {
			char filter_copy[MAX_CVAR_VALUE_STRING];
			Q_strncpyz(filter_copy, log_category_filter->string, sizeof(filter_copy));
			char *token = strtok(filter_copy, ",");
			
			while (token) {
				qboolean enable = qtrue;
				if (token[0] == '-') {
					enable = qfalse;
					token++;
				}
				
				for (int i = 0; i < LOG_CATEGORY_COUNT; i++) {
					if (!Q_stricmp(token, category_names[i])) {
						log_state.category_enabled[i] = enable;
						break;
					}
				}
				
				token = strtok(NULL, ",");
			}
		}
	}

	// Safety net: never allow file logging to be enabled implicitly.
	// If the user *really* wants file logging, they can set log_output at runtime,
	// but the engine startup path will not touch the filesystem via Q_Log_WriteToFile.
	log_state.output_flags &= ~LOG_OUTPUT_FILE;
	
#ifndef _WIN32
	// Initialize syslog if needed
	if (log_state.output_flags & LOG_OUTPUT_SYSLOG) {
		openlog("idtech3", LOG_PID | LOG_CONS, LOG_USER);
	}
#endif
	
	log_state.initialized = qtrue;
	
	Q_LogInfo(LOG_CATEGORY_GENERAL, "Structured logging system initialized");
}

/*
================
Q_Log_Shutdown
================
*/
void Q_Log_Shutdown(void) {
	if (!log_state.initialized) {
		return;
	}
	
	Q_Log_Flush();
	
	if (log_state.file_handle != FS_INVALID_HANDLE) {
		FS_FCloseFile(log_state.file_handle);
		log_state.file_handle = FS_INVALID_HANDLE;
	}
	
#ifndef _WIN32
	if (log_state.output_flags & LOG_OUTPUT_SYSLOG) {
		closelog();
	}
#endif
	
	log_state.initialized = qfalse;
}

/*
================
Q_Log_SetLevel
================
*/
void Q_Log_SetLevel(log_level_t level) {
	if (level >= 0 && level < LOG_LEVEL_COUNT) {
		log_state.global_level = level;
		if (log_level) {
			Cvar_SetIntegerValue("log_level", level);
		}
	}
}

/*
================
Q_Log_SetCategoryEnabled
================
*/
void Q_Log_SetCategoryEnabled(log_category_t category, qboolean enabled) {
	if (category >= 0 && category < LOG_CATEGORY_COUNT) {
		log_state.category_enabled[category] = enabled;
	}
}

/*
================
Q_Log_SetCategoryLevel
================
*/
void Q_Log_SetCategoryLevel(log_category_t category, log_level_t level) {
	if (category >= 0 && category < LOG_CATEGORY_COUNT && level >= 0 && level < LOG_LEVEL_COUNT) {
		log_state.category_level[category] = level;
	}
}

/*
================
Q_Log_IsCategoryEnabled
================
*/
qboolean Q_Log_IsCategoryEnabled(log_category_t category) {
	if (category >= 0 && category < LOG_CATEGORY_COUNT) {
		return log_state.category_enabled[category];
	}
	return qfalse;
}

/*
================
Q_Log_GetCategoryLevel
================
*/
log_level_t Q_Log_GetCategoryLevel(log_category_t category) {
	if (category >= 0 && category < LOG_CATEGORY_COUNT) {
		return log_state.category_level[category];
	}
	return LOG_LEVEL_DEBUG;
}

/*
================
Q_Log_SetFormat
================
*/
void Q_Log_SetFormat(log_format_t format) {
	if (format >= 0 && format < LOG_FORMAT_COUNT) {
		log_state.format = format;
		if (log_format) {
			Cvar_SetIntegerValue("log_format", format);
		}
	}
}

/*
================
Q_Log_SetOutput
================
*/
void Q_Log_SetOutput(int output_flags) {
	log_state.output_flags = output_flags;
	if (log_output) {
		Cvar_SetIntegerValue("log_output", output_flags);
	}
}

/*
================
Q_Log_SetFile
================
*/
void Q_Log_SetFile(const char *filename) {
	if (filename && filename[0]) {
		Q_strncpyz(log_state.filename, filename, sizeof(log_state.filename));
		
		// Close old file
		if (log_state.file_handle != FS_INVALID_HANDLE) {
			FS_FCloseFile(log_state.file_handle);
			log_state.file_handle = FS_INVALID_HANDLE;
		}
		
		if (log_file) {
			Cvar_Set("log_file", filename);
		}
	}
}

/*
================
Q_Log_SetRotationSize
================
*/
void Q_Log_SetRotationSize(int size_mb) {
	log_state.rotation_size_mb = size_mb;
	if (log_rotation_size) {
		Cvar_SetIntegerValue("log_rotation_size", size_mb);
	}
}

/*
================
Q_Log_SetRotationTime
================
*/
void Q_Log_SetRotationTime(int hours) {
	log_state.rotation_time_hours = hours;
	if (hours > 0) {
		time_t now = time(NULL);
		log_state.rotation_time = now + (hours * 3600);
	}
	if (log_rotation_time) {
		Cvar_SetIntegerValue("log_rotation_time", hours);
	}
}

/*
================
Q_Log_Flush
================
*/
void Q_Log_Flush(void) {
	if (log_state.file_handle != FS_INVALID_HANDLE) {
		FS_ForceFlush(log_state.file_handle);
	}
}

/*
================
Q_Log_ComPrintf
Compatibility layer for Com_Printf
================
*/
void QDECL Q_Log_ComPrintf(const char *fmt, ...) {
	va_list argptr;
	char message[MAX_LOG_MESSAGE];
	
	va_start(argptr, fmt);
	Q_vsnprintf(message, sizeof(message), fmt, argptr);
	va_end(argptr);
	
	Q_Log(LOG_LEVEL_INFO, LOG_CATEGORY_GENERAL, "compat", 0, "Com_Printf", "%s", message);
}

/*
================
Q_Log_ComDPrintf
Compatibility layer for Com_DPrintf
================
*/
void QDECL Q_Log_ComDPrintf(const char *fmt, ...) {
	extern cvar_t *com_developer;
	if (!com_developer || !com_developer->integer) {
		return;
	}
	
	va_list argptr;
	char message[MAX_LOG_MESSAGE];
	
	va_start(argptr, fmt);
	Q_vsnprintf(message, sizeof(message), fmt, argptr);
	va_end(argptr);
	
	Q_Log(LOG_LEVEL_DEBUG, LOG_CATEGORY_GENERAL, "compat", 0, "Com_DPrintf", "%s", message);
}

/*
================
Q_Log_IsEnabled
Check if logging is enabled (for backward compatibility)
================
*/
qboolean Q_Log_IsEnabled(void) {
	return log_state.initialized && log_enable && log_enable->integer;
}

