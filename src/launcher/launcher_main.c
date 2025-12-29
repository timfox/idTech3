/*
===========================================================================
Game Launcher - Native C executable for launching the engine

Provides:
- Auto-detection of content directories
- Content validation before launch
- Mod selection interface
- Proper path setup
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define chdir _chdir
#define getcwd _getcwd
#define execv _execv
#define stat _stat
#define S_ISDIR(mode) ((mode) & _S_IFDIR)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

#define MAX_PATH 1024
#define MAX_ARGS 64

// Simple path utilities
static int path_exists(const char *path) {
	struct stat st;
	return (stat(path, &st) == 0);
}

static int is_directory(const char *path) {
	struct stat st;
	if (stat(path, &st) != 0) {
		return 0;
	}
	return S_ISDIR(st.st_mode);
}

// Cross-platform path normalization
static void normalize_path(char *path) {
#ifdef _WIN32
	char *p = path;
	while (*p) {
		if (*p == '/') *p = '\\';
		p++;
	}
#else
	char *p = path;
	while (*p) {
		if (*p == '\\') *p = '/';
		p++;
	}
#endif
}

/*
=================
Launcher_FindContentDir

Locates game content directory by checking common locations.
Returns path if found, NULL otherwise.
=================
*/
static const char *Launcher_FindContentDir(const char *execPath) {
	static char contentDir[MAX_PATH];
	const char *candidates[] = {
		"release",
		"base",
		".",
		NULL
	};
	
	int i;
	char testPath[MAX_PATH];
	
	// Get directory containing executable
	char *execDir = strdup(execPath);
	char *lastSep = strrchr(execDir, PATH_SEPARATOR);
	if (!lastSep) {
		lastSep = strrchr(execDir, PATH_SEPARATOR == '\\' ? '/' : '\\');
	}
	if (lastSep) {
		*lastSep = '\0';
	} else {
		execDir[0] = '.';
		execDir[1] = '\0';
	}

	// Try candidates relative to executable directory
	for (i = 0; candidates[i] != NULL; i++) {
		snprintf(testPath, sizeof(testPath), "%s%s%s", execDir, PATH_SEPARATOR_STR, candidates[i]);
		normalize_path(testPath);
		if (is_directory(testPath)) {
			// Check for pak files
			char pakPath[MAX_PATH];
			snprintf(pakPath, sizeof(pakPath), "%s%spak0.pk3", testPath, PATH_SEPARATOR_STR);
			normalize_path(pakPath);
			if (path_exists(pakPath)) {
				strncpy(contentDir, testPath, sizeof(contentDir) - 1);
				contentDir[sizeof(contentDir) - 1] = '\0';
				free(execDir);
				return contentDir;
			}
		}
	}
	
	// Try absolute paths
	for (i = 0; candidates[i] != NULL; i++) {
		if (is_directory(candidates[i])) {
			char pakPath[MAX_PATH];
			snprintf(pakPath, sizeof(pakPath), "%s/pak0.pk3", candidates[i]);
			if (path_exists(pakPath)) {
				strncpy(contentDir, candidates[i], sizeof(contentDir) - 1);
				contentDir[sizeof(contentDir) - 1] = '\0';
				free(execDir);
				return contentDir;
			}
		}
	}
	
	free(execDir);
	return NULL;
}

/*
=================
Launcher_ValidateContent

Checks for required content files.
Returns 1 if valid, 0 otherwise.
=================
*/
static int Launcher_ValidateContent(const char *contentDir) {
	char pakPath[MAX_PATH];
	int foundPak = 0;
	
	if (!contentDir) {
		return 0;
	}
	
	// Check for at least one pak file
	snprintf(pakPath, sizeof(pakPath), "%s%spak0.pk3", contentDir, PATH_SEPARATOR_STR);
	normalize_path(pakPath);
	if (path_exists(pakPath)) {
		foundPak = 1;
	} else {
		// Try other common pak files
		int i;
		for (i = 1; i <= 9 && !foundPak; i++) {
			snprintf(pakPath, sizeof(pakPath), "%s%spak%d.pk3", contentDir, PATH_SEPARATOR_STR, i);
			normalize_path(pakPath);
			if (path_exists(pakPath)) {
				foundPak = 1;
			}
		}
	}
	
	return foundPak;
}

/*
=================
Launcher_SelectMod

Interactive mod selection (simplified - just lists available mods).
Returns selected mod name or NULL for base game.
=================
*/
static const char *Launcher_SelectMod(const char *basePath, int argc, char **argv) {
    (void)basePath;
	int i;
	
	// Check command line for mod selection
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "+set") == 0 && i + 1 < argc && 
		    strcmp(argv[i + 1], "fs_game") == 0 && i + 2 < argc) {
			return argv[i + 2];
		}
		if (strncmp(argv[i], "+set", 4) == 0 && strstr(argv[i], "fs_game")) {
			// Handle +set fs_game modname format
			char *equals = strchr(argv[i], '=');
			if (equals) {
				return equals + 1;
			}
		}
	}
	
	return NULL; // Base game
}

/*
=================
Launcher_LaunchEngine

Executes the engine with proper arguments and environment.
=================
*/
static int Launcher_LaunchEngine(const char *enginePath, const char *contentDir, 
                                  const char *modName, int argc, char **argv) {
	char *new_argv[MAX_ARGS];
	int new_argc = 0;
	int i;
	
	// Build new argument list
	new_argv[new_argc++] = (char *)enginePath;
	
	// Add content directory as basepath if specified
	if (contentDir) {
		new_argv[new_argc++] = strdup("+set");
		new_argv[new_argc++] = strdup("fs_basepath");
		new_argv[new_argc++] = strdup(contentDir);
	}
	
	// Add mod selection if specified
	if (modName && *modName) {
		new_argv[new_argc++] = strdup("+set");
		new_argv[new_argc++] = strdup("fs_game");
		new_argv[new_argc++] = strdup(modName);
	}
	
	// Pass through remaining arguments
	for (i = 1; i < argc && new_argc < MAX_ARGS - 1; i++) {
		// Skip arguments we've already processed
		if (strcmp(argv[i], "+set") == 0 && i + 1 < argc && 
		    strcmp(argv[i + 1], "fs_game") == 0) {
			i += 2; // Skip +set fs_game and value
			continue;
		}
		new_argv[new_argc++] = argv[i];
	}
	
	new_argv[new_argc] = NULL;
	
	// Change to content directory if specified
	if (contentDir) {
		if (chdir(contentDir) != 0) {
			fprintf(stderr, "Warning: Could not change to content directory: %s\n", contentDir);
		}
	}
	
	// Execute engine
	execv(enginePath, new_argv);
	
	// If execv returns, it failed
	perror("execv failed");
	return 1;
}

/*
=================
main

Launcher entry point
=================
*/
int main(int argc, char **argv) {
	const char *execPath = argv[0];
	const char *contentDir;
	const char *modName;
	char enginePath[MAX_PATH];
	
	printf("idTech3 Game Launcher\n");
	printf("====================\n\n");
	
	// Find engine binary (assume it's in the same directory as launcher)
	char *lastSep = strrchr(execPath, PATH_SEPARATOR);
	if (!lastSep) {
		lastSep = strrchr(execPath, PATH_SEPARATOR == '\\' ? '/' : '\\');
	}
	if (lastSep) {
		int len = lastSep - execPath;
#ifdef _WIN32
		snprintf(enginePath, sizeof(enginePath), "%.*s\\idtech3.x86_64.exe", len, execPath);
#else
		snprintf(enginePath, sizeof(enginePath), "%.*s/idtech3.x86_64", len, execPath);
#endif
	} else {
#ifdef _WIN32
		strncpy(enginePath, "idtech3.x86_64.exe", sizeof(enginePath) - 1);
#else
		strncpy(enginePath, "idtech3.x86_64", sizeof(enginePath) - 1);
#endif
	}

	// Check if engine exists
	if (!path_exists(enginePath)) {
		// Try release directory
#ifdef _WIN32
		snprintf(enginePath, sizeof(enginePath), "release\\idtech3.x86_64.exe");
#else
		snprintf(enginePath, sizeof(enginePath), "release/idtech3.x86_64");
#endif
		if (!path_exists(enginePath)) {
			fprintf(stderr, "Error: Engine binary not found: %s\n", enginePath);
#ifdef _WIN32
			fprintf(stderr, "  Expected: idtech3.x86_64.exe or release\\idtech3.x86_64.exe\n");
#else
			fprintf(stderr, "  Expected: idtech3.x86_64 or release/idtech3.x86_64\n");
#endif
			return 1;
		}
	}
	
	printf("Engine: %s\n", enginePath);
	
	// Find content directory
	contentDir = Launcher_FindContentDir(execPath);
	if (!contentDir) {
		fprintf(stderr, "Warning: Content directory not found automatically.\n");
		fprintf(stderr, "  Looking for: release/, base/, or current directory with pak files\n");
		fprintf(stderr, "  Continuing anyway...\n");
	} else {
		printf("Content: %s\n", contentDir);
		
		// Validate content
		if (!Launcher_ValidateContent(contentDir)) {
			fprintf(stderr, "Warning: No pak files found in content directory.\n");
			fprintf(stderr, "  The engine may not work without game content.\n");
			fprintf(stderr, "  Place .pk3 files in: %s\n", contentDir);
		} else {
			printf("Content validation: OK\n");
		}
	}
	
	// Select mod
	modName = Launcher_SelectMod(contentDir ? contentDir : ".", argc, argv);
	if (modName) {
		printf("Mod: %s\n", modName);
	} else {
		printf("Mod: base game\n");
	}
	
	printf("\nLaunching engine...\n\n");
	
	// Launch engine
	return Launcher_LaunchEngine(enginePath, contentDir, modName, argc, argv);
}
