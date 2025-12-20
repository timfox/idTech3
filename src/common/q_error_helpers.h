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

#ifndef __Q_ERROR_HELPERS_H__
#define __Q_ERROR_HELPERS_H__

#include "q_shared.h"

/**
 * Error handling helper macros for consistent error reporting
 */

/**
 * @brief Check a condition and return early if it fails
 * @param condition Condition to check (if false, returns)
 * @param return_value Value to return on failure
 * @param error_msg Error message to print
 */
#define RETURN_ON_ERROR(condition, return_value, error_msg) \
	do { \
		if (!(condition)) { \
			Com_Printf("ERROR: %s\n", error_msg); \
			return (return_value); \
		} \
	} while(0)

/**
 * @brief Check a condition and call Com_Error if it fails
 * @param condition Condition to check (if false, calls Com_Error)
 * @param error_code Error code (ERR_DROP, ERR_FATAL, etc.)
 * @param error_msg Error message format string
 * @param ... Format arguments
 */
#define ERROR_ON_FAILURE(condition, error_code, error_msg, ...) \
	do { \
		if (!(condition)) { \
			Com_Error(error_code, error_msg, ##__VA_ARGS__); \
		} \
	} while(0)

/**
 * @brief Check for NULL pointer and return early if NULL
 * @param ptr Pointer to check
 * @param return_value Value to return if NULL
 * @param error_msg Error message to print
 */
#define RETURN_IF_NULL(ptr, return_value, error_msg) \
	RETURN_ON_ERROR((ptr) != NULL, return_value, error_msg)

/**
 * @brief Check for NULL pointer and call Com_Error if NULL
 * @param ptr Pointer to check
 * @param error_code Error code
 * @param error_msg Error message format string
 * @param ... Format arguments
 */
#define ERROR_IF_NULL(ptr, error_code, error_msg, ...) \
	ERROR_ON_FAILURE((ptr) != NULL, error_code, error_msg, ##__VA_ARGS__)

#endif // __Q_ERROR_HELPERS_H__

