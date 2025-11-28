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

#ifndef __Q_LOG_H__
#define __Q_LOG_H__

#include "q_shared.h"

// Log levels
typedef enum {
	LOG_LEVEL_DEBUG = 0,
	LOG_LEVEL_INFO = 1,
	LOG_LEVEL_WARN = 2,
	LOG_LEVEL_ERROR = 3,
	LOG_LEVEL_FATAL = 4,
	LOG_LEVEL_COUNT
} log_level_t;

// Log categories
typedef enum {
	LOG_CATEGORY_GENERAL = 0,
	LOG_CATEGORY_CLIENT,
	LOG_CATEGORY_SERVER,
	LOG_CATEGORY_RENDERER,
	LOG_CATEGORY_NETWORK,
	LOG_CATEGORY_FILESYSTEM,
	LOG_CATEGORY_SOUND,
	LOG_CATEGORY_INPUT,
	LOG_CATEGORY_PHYSICS,
	LOG_CATEGORY_AI,
	LOG_CATEGORY_SCRIPT,
	LOG_CATEGORY_MEMORY,
	LOG_CATEGORY_COUNT
} log_category_t;

// Output formats
typedef enum {
	LOG_FORMAT_TEXT = 0,
	LOG_FORMAT_JSON = 1,
	LOG_FORMAT_COUNT
} log_format_t;

// Output destinations (bit flags)
#define LOG_OUTPUT_CONSOLE	(1 << 0)
#define LOG_OUTPUT_FILE		(1 << 1)
#define LOG_OUTPUT_SYSLOG	(1 << 2)
#define LOG_OUTPUT_ALL		(LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE | LOG_OUTPUT_SYSLOG)

// Forward declarations
struct cvar_s;

// Initialize the logging system
void Q_Log_Init(void);
void Q_Log_Shutdown(void);

// Main logging function
void QDECL Q_Log(log_level_t level, log_category_t category, const char *file, int line, const char *func, const char *fmt, ...) FORMAT_PRINTF(6, 7);

// Convenience macros
#define Q_LogDebug(category, fmt, ...)	Q_Log(LOG_LEVEL_DEBUG, category, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogInfo(category, fmt, ...)	Q_Log(LOG_LEVEL_INFO, category, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogWarn(category, fmt, ...)	Q_Log(LOG_LEVEL_WARN, category, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogError(category, fmt, ...)	Q_Log(LOG_LEVEL_ERROR, category, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogFatal(category, fmt, ...)	Q_Log(LOG_LEVEL_FATAL, category, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// Category-specific convenience macros
#define Q_LogClient(level, fmt, ...)	Q_Log(level, LOG_CATEGORY_CLIENT, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogServer(level, fmt, ...)	Q_Log(level, LOG_CATEGORY_SERVER, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogRenderer(level, fmt, ...)	Q_Log(level, LOG_CATEGORY_RENDERER, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define Q_LogNetwork(level, fmt, ...)	Q_Log(level, LOG_CATEGORY_NETWORK, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// Filtering functions
void Q_Log_SetLevel(log_level_t level);
void Q_Log_SetCategoryEnabled(log_category_t category, qboolean enabled);
void Q_Log_SetCategoryLevel(log_category_t category, log_level_t level);
qboolean Q_Log_IsCategoryEnabled(log_category_t category);
log_level_t Q_Log_GetCategoryLevel(log_category_t category);

// Output configuration
void Q_Log_SetFormat(log_format_t format);
void Q_Log_SetOutput(int output_flags);
void Q_Log_SetFile(const char *filename);
void Q_Log_SetRotationSize(int size_mb);
void Q_Log_SetRotationTime(int hours);
void Q_Log_Flush(void);

// Get category name string
const char *Q_Log_GetCategoryName(log_category_t category);
const char *Q_Log_GetLevelName(log_level_t level);

// Check if logging is enabled (for backward compatibility)
qboolean Q_Log_IsEnabled(void);

// Compatibility layer - redirects Com_Printf to structured logger
void QDECL Q_Log_ComPrintf(const char *fmt, ...) FORMAT_PRINTF(1, 2);
void QDECL Q_Log_ComDPrintf(const char *fmt, ...) FORMAT_PRINTF(1, 2);

#endif // __Q_LOG_H__

