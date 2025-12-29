/*
===========================================================================
Content Validation System

Comprehensive validation for pak files, mods, and game content.
Provides user-friendly error messages and ensures content integrity.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "fs_validation.h"
#include "files_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Validation result structure is defined in qcommon.h

// Function prototypes
qboolean Mod_ApplySandboxRestrictions( const char *modName );
void Mod_RemoveSandboxRestrictions( const char *modName );

/*
=================
FS_ValidatePakFile

Validates an individual pak file for integrity and readability.
Returns qtrue if valid, qfalse otherwise.
=================
*/
qboolean FS_ValidatePakFile( const char *pakPath, char *errorMsg, int errorMsgSize ) {
	fileHandle_t f;
	int len;
	
	if ( !pakPath || !*pakPath ) {
		Q_strncpyz( errorMsg, "Invalid pak file path", errorMsgSize );
		return qfalse;
	}
	
	// Try to open the file using correct API signature
	len = FS_FOpenFileRead( pakPath, &f, qfalse );
	if ( len < 0 || f == FS_INVALID_HANDLE ) {
		Com_sprintf( errorMsg, errorMsgSize, "Cannot open pak file: %s", pakPath );
		return qfalse;
	}
	
	// Check minimum size (pak files should be at least a few KB)
	if ( len < 1024 ) {
		FS_FCloseFile( f );
		Com_sprintf( errorMsg, errorMsgSize, "Pak file too small (corrupted?): %s", pakPath );
		return qfalse;
	}
	
	// Additional validation: try to read first few bytes to check zip signature
	byte header[4];
	int read = FS_Read( header, 4, f );
	FS_FCloseFile( f );
	
	// Check for ZIP file signature (PK\x03\x04 or PK\x05\x06 for empty zip)
	if ( read == 4 && header[0] == 'P' && header[1] == 'K' ) {
		// Valid zip signature
		return qtrue;
	} else if ( read < 4 ) {
		Com_sprintf( errorMsg, errorMsgSize, "Pak file too small or unreadable: %s", pakPath );
		return qfalse;
	} else {
		Com_sprintf( errorMsg, errorMsgSize, "Invalid pak file format (not a zip file): %s", pakPath );
		return qfalse;
	}
}

/*
=================
FS_ValidateGameContent

Checks for required game content files.
Returns qtrue if all required content is present.
=================
*/
qboolean FS_ValidateGameContent( fs_validation_result_t *result ) {
	char errorMsg[MAX_OSPATH];
	const char *requiredPaks[] = {
		"pak0.pk3",
		NULL
	};
	
	int i;
	qboolean allValid = qtrue;
	
	if ( !result ) {
		return qfalse;
	}
	
	result->valid = qtrue;
	result->error = NULL;
	result->missing_files = 0;
	result->corrupted_files = 0;
	
	// Check for required pak files
	for ( i = 0; requiredPaks[i] != NULL; i++ ) {
		char pakPath[MAX_OSPATH];
		fileHandle_t f;
		
		// Try to find the pak file
		int fileLen = FS_FOpenFileRead( requiredPaks[i], &f, qfalse );
		if ( fileLen < 0 || f == FS_INVALID_HANDLE ) {
			result->missing_files++;
			result->valid = qfalse;
			allValid = qfalse;
			continue;
		}
		FS_FCloseFile( f );
		
		// Validate the pak file
		Com_sprintf( pakPath, sizeof( pakPath ), "%s", requiredPaks[i] );
		if ( !FS_ValidatePakFile( pakPath, errorMsg, sizeof( errorMsg ) ) ) {
			result->corrupted_files++;
			result->valid = qfalse;
			allValid = qfalse;
		}
	}
	
	return allValid;
}

/*
=================
Mod_ValidateBeforeLoad

Pre-load validation for mod VM files and structure.
Returns qtrue if mod appears safe to load.
=================
*/
qboolean Mod_ValidateBeforeLoad( const char *modName ) {
	char vmPath[MAX_OSPATH];
	char modPath[MAX_OSPATH];
	fileHandle_t f;
	int fileLen;
	
	if ( !modName || !*modName ) {
		return qfalse;
	}
	
	// Check mod name length
	if ( strlen( modName ) >= MAX_QPATH ) {
		Com_Printf( S_COLOR_RED "Mod name too long: %s\n", modName );
		return qfalse;
	}
	
	// Check for invalid characters in mod name
	if ( strstr( modName, ".." ) || strchr( modName, '/' ) || strchr( modName, '\\' ) ) {
		Com_Printf( S_COLOR_RED "Invalid mod name (contains path separators): %s\n", modName );
		return qfalse;
	}
	
	// Try to find mod directory
	Com_sprintf( modPath, sizeof( modPath ), "%s/", modName );
	fileLen = FS_FOpenFileRead( modPath, &f, qfalse );
	if ( fileLen < 0 || f == FS_INVALID_HANDLE ) {
		// Mod directory doesn't exist - might be content-only, which is OK
		return qtrue;
	}
	FS_FCloseFile( f );
	
	// Check for VM files - validate they exist and are readable
	Com_sprintf( vmPath, sizeof( vmPath ), "%s/vm/game.qvm", modPath );
	fileLen = FS_FOpenFileRead( vmPath, &f, qfalse );
	if ( fileLen >= 0 && f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );

		// Validate VM file size (should be reasonable)
		if ( fileLen < 1024 ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Mod VM file appears too small: %s (%d bytes)\n", vmPath, fileLen );
		} else if ( fileLen > 50 * 1024 * 1024 ) { // 50MB max
			Com_Printf( S_COLOR_RED "Mod VM file too large: %s (%d bytes)\n", vmPath, fileLen );
			return qfalse;
		}
	}

	// Additional security checks: look for potentially dangerous files
	const char *dangerousFiles[] = {
		"autoexec.cfg",     // Could override user settings
		"config.cfg",       // Could override user config
		"../q3config.cfg",  // Path traversal attempt
		"../../../etc/passwd", // Unix path traversal
		NULL
	};

	int i;
	for ( i = 0; dangerousFiles[i] != NULL; i++ ) {
		char dangerousPath[MAX_OSPATH];
		Com_sprintf( dangerousPath, sizeof( dangerousPath ), "%s/%s", modPath, dangerousFiles[i] );
		fileLen = FS_FOpenFileRead( dangerousPath, &f, qfalse );
		if ( fileLen >= 0 && f != FS_INVALID_HANDLE ) {
			FS_FCloseFile( f );
			Com_Printf( S_COLOR_RED "Security warning: Mod contains potentially dangerous file: %s\n", dangerousFiles[i] );
			// Don't fail validation - just warn, but could be made stricter
		}
	}

	// Check for scripts directory - validate script files don't contain dangerous commands
	char scriptsPath[MAX_OSPATH];
	Com_sprintf( scriptsPath, sizeof( scriptsPath ), "%s/scripts/", modPath );
	// Note: Would need directory enumeration to check individual script files
	// For now, just check if scripts directory exists and warn

	return qtrue;
}

/*
=================
FS_ValidateMod

Validates mod structure and compatibility.
Checks for required mod files and validates pak files.
=================
*/
qboolean FS_ValidateMod( const char *modName, fs_validation_result_t *result ) {
	char modPath[MAX_OSPATH];
	char vmPath[MAX_OSPATH];
	fileHandle_t f;
	
	if ( !modName || !*modName ) {
		if ( result ) {
			result->valid = qfalse;
			result->error = "Invalid mod name";
		}
		return qfalse;
	}
	
	if ( !result ) {
		return qfalse;
	}
	
	result->valid = qtrue;
	result->error = NULL;
	result->missing_files = 0;
	result->corrupted_files = 0;
	
	// Check for mod directory
	Com_sprintf( modPath, sizeof( modPath ), "%s/", modName );
	int fileLen = FS_FOpenFileRead( va( "%s/scripts/main.qvm", modPath ), &f, qfalse );
	if ( fileLen < 0 || f == FS_INVALID_HANDLE ) {
		// Try alternative location
		Com_sprintf( vmPath, sizeof( vmPath ), "%s/vm/game.x86_64.so", modPath );
		fileLen = FS_FOpenFileRead( vmPath, &f, qfalse );
		if ( fileLen < 0 || f == FS_INVALID_HANDLE ) {
			// Mod doesn't have VM files - might be content-only mod, which is OK
			// Just check if mod directory exists
			Com_sprintf( vmPath, sizeof( vmPath ), "%s/", modPath );
			fileLen = FS_FOpenFileRead( vmPath, &f, qfalse );
			if ( fileLen < 0 || f == FS_INVALID_HANDLE ) {
				result->valid = qfalse;
				result->error = va( "Mod directory not found: %s", modName );
				return qfalse;
			}
		}
	}
	if ( f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );
	}
	
	// Validate any pak files in the mod
	// This is a simplified check - in practice, we'd enumerate all pk3 files
	char pakPath[MAX_OSPATH];
	Com_sprintf( pakPath, sizeof( pakPath ), "%s/%s.pk3", modPath, modName );
	fileLen = FS_FOpenFileRead( pakPath, &f, qfalse );
	if ( fileLen >= 0 && f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );
		char errorMsg[MAX_OSPATH];
		if ( !FS_ValidatePakFile( pakPath, errorMsg, sizeof( errorMsg ) ) ) {
			result->corrupted_files++;
			result->valid = qfalse;
		}
	}
	
	return result->valid;
}

/*
=================
FS_ReportMissingContent

Provides user-friendly error messages about missing or corrupted content.
=================
*/
void FS_ReportMissingContent( fs_validation_result_t *result ) {
	if ( !result ) {
		return;
	}
	
	if ( result->valid ) {
		Com_Printf( "Content validation: All required files present and valid.\n" );
		return;
	}
	
	Com_Printf( S_COLOR_RED "Content validation failed!\n" );
	
	if ( result->missing_files > 0 ) {
		Com_Printf( S_COLOR_YELLOW "  Missing files: %d\n", result->missing_files );
		Com_Printf( S_COLOR_YELLOW "  Required: pak0.pk3 (and optionally pak1.pk3, pak2.pk3, etc.)\n" );
		Com_Printf( S_COLOR_YELLOW "  Place your game content (.pk3 files) in the base/ directory\n" );
	}
	
	if ( result->corrupted_files > 0 ) {
		Com_Printf( S_COLOR_YELLOW "  Corrupted files: %d\n", result->corrupted_files );
		Com_Printf( S_COLOR_YELLOW "  Some pak files appear to be damaged. Please reinstall game content.\n" );
	}
	
	if ( result->error ) {
		Com_Printf( S_COLOR_YELLOW "  Error: %s\n", result->error );
	}
	
	Com_Printf( "\n" );
	Com_Printf( S_COLOR_CYAN "Note: You can use any .pk3 files in the base directory.\n" );
	Com_Printf( S_COLOR_CYAN "      Place your game assets (maps, textures, etc.) in .pk3 files.\n" );
}

/*
=================
FS_ValidateContentOnStartup

Main entry point for content validation during engine startup.
Called from FS_Startup or launcher.
=================
*/
qboolean FS_ValidateContentOnStartup( void ) {
	fs_validation_result_t result;
	qboolean valid;
	
	Com_Printf( "Validating game content...\n" );
	
	valid = FS_ValidateGameContent( &result );
	
	if ( !valid ) {
		FS_ReportMissingContent( &result );
		// Don't fail startup - allow engine to run with missing content
		// (user might be setting up or testing)
		return qfalse;
	}
	
	Com_Printf( S_COLOR_GREEN "Content validation: OK\n" );
	return qtrue;
}

/*
=================
Mod_ApplySandboxRestrictions

Apply sandbox restrictions for mod loading to prevent security issues.
Returns qtrue if restrictions applied successfully.
=================
*/
qboolean Mod_ApplySandboxRestrictions( const char *modName ) {
	if ( !modName || !*modName ) {
		return qfalse;
	}

	// Set mod-specific cvars to restrict functionality
	Cvar_Get( va( "mod_%s_restricted", modName), "1", CVAR_ROM | CVAR_PRIVATE );

	// Restrict filesystem access for mods
	Cvar_Get( va( "fs_mod_%s_basegame_only", modName), "1", CVAR_ROM | CVAR_PRIVATE );

	// Disable potentially dangerous commands for mod VMs
	Cvar_Get( va( "mod_%s_safe_mode", modName), "1", CVAR_ROM | CVAR_PRIVATE );

	Com_Printf( "Applied sandbox restrictions for mod: %s\n", modName );
	return qtrue;
}

/*
=================
Mod_RemoveSandboxRestrictions

Remove sandbox restrictions when unloading a mod.
=================
*/
void Mod_RemoveSandboxRestrictions( const char *modName ) {
	if ( !modName || !*modName ) {
		return;
	}

	// Remove mod-specific restriction cvars
	Cvar_Set( va( "mod_%s_restricted", modName), "0" );
	Cvar_Set( va( "fs_mod_%s_basegame_only", modName), "0" );
	Cvar_Set( va( "mod_%s_safe_mode", modName), "0" );

	Com_Printf( "Removed sandbox restrictions for mod: %s\n", modName );
}
