/*
===========================================================================
Unit tests for Virtual Filesystem v2
===========================================================================
*/

#include "test_framework.h"
#include "../src/common/q_shared.h"
#include "../src/common/files_internal.h"  // For pack_t, fileHandleData_t, etc.
#include "../src/common/files_v2.h"
#include <string.h>
#include <strings.h>  // for strcasecmp, strncasecmp
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// Undefine macros that we need to implement as functions
#undef Com_Memset

// Forward declarations for mock functions
void *Com_Memset(void *dest, int val, size_t count);
float Q_atof(const char *str);
qboolean FS_PakIsPure(const pack_t *pack);
int FS_OpenFileInPak(fileHandle_t *file, pack_t *pak, fileInPack_t *pakFile, qboolean uniqueFILE);
pack_t *FS_LoadZipFile(const char *zipfile);
char *FS_BuildOSPath(const char *base, const char *game, const char *qpath);
int FS_FileLength(FILE *h);
fileHandle_t FS_HandleForFile(void);
void FS_InitHandle(fileHandleData_t *fd);
qboolean FS_FilenameCompare(const char *s1, const char *s2);
FILE *Sys_FOpen(const char *ospath, const char *mode);
int Cmd_Argc(void);
const char *Cmd_Argv(int arg);
void Cmd_AddCommand(const char *cmd_name, xcommand_t function);

// Mock functions needed by files_v2.c
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;  // Suppress unused parameter warning
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	fprintf(stderr, "\n");
	exit(1);
}

void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

void Com_DPrintf(const char *fmt, ...) {
	// Silent in tests
	(void)fmt;
}

// Mock memory allocation
static char mock_memory[1024 * 1024];  // 1MB mock heap
static size_t mock_offset = 0;

// Mock fsh array (declared extern in files_internal.h, defined in files.c)
// We need to provide a definition for the test
fileHandleData_t fsh[MAX_FILE_HANDLES];

// Mock fs_searchpaths (declared extern in files_internal.h, defined in files.c)
// We need to provide a definition for the test
searchpath_t *fs_searchpaths = NULL;

// Mock fs_checksumFeed (declared extern in files_internal.h, defined in files.c)
int fs_checksumFeed = 0;

void *Z_TagMalloc(int size, memtag_t tag) {
	(void)tag;
	if (mock_offset + size > sizeof(mock_memory)) {
		return NULL;
	}
	void *ptr = mock_memory + mock_offset;
	mock_offset += size;
	return ptr;
}

void Z_Free(void *ptr) {
	// Simple mock - in real implementation would track allocations
	(void)ptr;
}

// Mock Com_Memset (q_shared.c defines it as a macro, we need a function)
void *Com_Memset(void *dest, int val, size_t count) {
	return memset(dest, val, count);
}

// Note: Com_sprintf, Q_strncpyz, Q_stricmp, Q_stricmpn, Com_GenerateHashValue
// are already defined in q_shared.c, so we don't redefine them here

// Mock Q_atof
float Q_atof(const char *str) {
	return (float)atof(str);
}

// Mock functions from files.c that files_v2.c needs
qboolean FS_PakIsPure(const pack_t *pack) {
	(void)pack;
	return qfalse;
}

int FS_OpenFileInPak(fileHandle_t *file, pack_t *pak, fileInPack_t *pakFile, qboolean uniqueFILE) {
	(void)file;
	(void)pak;
	(void)pakFile;
	(void)uniqueFILE;
	return 0;
}

pack_t *FS_LoadZipFile(const char *zipfile) {
	(void)zipfile;
	return NULL;
}

char *FS_BuildOSPath(const char *base, const char *game, const char *qpath) {
	static char path[1024];
	snprintf(path, sizeof(path), "%s/%s/%s", base, game, qpath);
	return path;
}

int FS_FileLength(FILE *h) {
	if (!h) return 0;
	long pos = ftell(h);
	fseek(h, 0, SEEK_END);
	long len = ftell(h);
	fseek(h, pos, SEEK_SET);
	return (int)len;
}

fileHandle_t FS_HandleForFile(void) {
	static int handle = 1;
	return handle++;
}

void FS_InitHandle(fileHandleData_t *fd) {
	if (fd) {
		memset(fd, 0, sizeof(*fd));
	}
}

qboolean FS_FilenameCompare(const char *s1, const char *s2) {
	return (qboolean)(strcmp(s1, s2) == 0);
}

FILE *Sys_FOpen(const char *ospath, const char *mode) {
	(void)ospath;
	(void)mode;
	return NULL;  // Mock - return NULL for tests
}

// Mock Cmd functions
int Cmd_Argc(void) {
	return 0;
}

const char *Cmd_Argv(int arg) {
	(void)arg;
	return "";
}

void Cmd_AddCommand(const char *cmd_name, xcommand_t function) {
	(void)cmd_name;
	(void)function;
}

// ============================================================================
// Test Cases
// ============================================================================

TEST(mount_table_init_and_shutdown) {
	FS_MountTable_Init();
	ASSERT_TRUE(FS_MountTable_IsActive() == qfalse);  // No mounts yet
	
	fsMount_t *mount = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	ASSERT_TRUE(mount != NULL);
	
	ASSERT_TRUE(FS_Mount_Add(mount) == qtrue);
	ASSERT_TRUE(FS_MountTable_IsActive() == qtrue);
	
	FS_MountTable_Shutdown();
	ASSERT_TRUE(FS_MountTable_IsActive() == qfalse);
}

TEST(mount_create_and_destroy) {
	FS_MountTable_Init();
	
	fsMount_t *mount = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	ASSERT_TRUE(mount != NULL);
	ASSERT_TRUE(strcmp(mount->mountPoint, "test/mount") == 0);
	ASSERT_TRUE(mount->type == FS_MOUNT_DIR);
	ASSERT_TRUE(mount->priority == FS_PRIORITY_GAME);
	ASSERT_TRUE(mount->enabled == qtrue);
	ASSERT_TRUE(mount->writePolicy == FS_WRITE_DENY);
	
	FS_Mount_Destroy(mount);
	FS_MountTable_Shutdown();
}

TEST(mount_add_and_remove) {
	FS_MountTable_Init();
	
	fsMount_t *mount = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	ASSERT_TRUE(FS_Mount_Add(mount) == qtrue);
	
	fsMount_t *found = FS_Mount_Find("test/mount");
	ASSERT_TRUE(found == mount);
	
	ASSERT_TRUE(FS_Mount_Remove("test/mount") == qtrue);
	found = FS_Mount_Find("test/mount");
	ASSERT_TRUE(found == NULL);
	
	FS_Mount_Destroy(mount);
	FS_MountTable_Shutdown();
}

TEST(mount_priority_ordering) {
	FS_MountTable_Init();
	
	fsMount_t *mount1 = FS_Mount_Create("test/low", FS_MOUNT_DIR, FS_PRIORITY_FALLBACK);
	fsMount_t *mount2 = FS_Mount_Create("test/high", FS_MOUNT_DIR, FS_PRIORITY_SYSTEM);
	fsMount_t *mount3 = FS_Mount_Create("test/medium", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	
	// Add in reverse order
	ASSERT_TRUE(FS_Mount_Add(mount1) == qtrue);
	ASSERT_TRUE(FS_Mount_Add(mount2) == qtrue);
	ASSERT_TRUE(FS_Mount_Add(mount3) == qtrue);
	
	// Check ordering (highest priority first)
	fsMount_t *current = FS_Mount_Find("test/high");
	ASSERT_TRUE(current != NULL);
	ASSERT_TRUE(current->priority == FS_PRIORITY_SYSTEM);
	
	current = FS_Mount_Find("test/medium");
	ASSERT_TRUE(current != NULL);
	ASSERT_TRUE(current->priority == FS_PRIORITY_GAME);
	
	current = FS_Mount_Find("test/low");
	ASSERT_TRUE(current != NULL);
	ASSERT_TRUE(current->priority == FS_PRIORITY_FALLBACK);
	
	FS_Mount_Remove("test/high");
	FS_Mount_Remove("test/medium");
	FS_Mount_Remove("test/low");
	FS_Mount_Destroy(mount1);
	FS_Mount_Destroy(mount2);
	FS_Mount_Destroy(mount3);
	FS_MountTable_Shutdown();
}

TEST(mount_enable_disable) {
	FS_MountTable_Init();
	
	fsMount_t *mount = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	ASSERT_TRUE(FS_Mount_Add(mount) == qtrue);
	
	ASSERT_TRUE(mount->enabled == qtrue);
	ASSERT_TRUE(FS_Mount_SetEnabled("test/mount", qfalse) == qtrue);
	ASSERT_TRUE(mount->enabled == qfalse);
	ASSERT_TRUE(FS_Mount_SetEnabled("test/mount", qtrue) == qtrue);
	ASSERT_TRUE(mount->enabled == qtrue);
	
	FS_Mount_Remove("test/mount");
	FS_Mount_Destroy(mount);
	FS_MountTable_Shutdown();
}

TEST(write_policy_check) {
	FS_MountTable_Init();
	
	fsMount_t *mount = FS_Mount_Create("test/write", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	mount->writePolicy = FS_WRITE_ALLOW;
	ASSERT_TRUE(FS_Mount_Add(mount) == qtrue);
	
	fsMount_t *writeMount = NULL;
	ASSERT_TRUE(FS_WritePolicy_Check("test/file.txt", &writeMount) == qfalse);  // No write mount set
	
	ASSERT_TRUE(FS_Mount_SetWriteMount("test/write") == qtrue);
	ASSERT_TRUE(FS_WritePolicy_Check("test/file.txt", &writeMount) == qtrue);
	ASSERT_TRUE(writeMount == mount);
	
	FS_Mount_Remove("test/write");
	FS_Mount_Destroy(mount);
	FS_MountTable_Shutdown();
}

TEST(sandbox_rules) {
	fsSandboxRules_t rules;
	
	FS_Sandbox_InitDefaultRules(&rules);
	ASSERT_TRUE(rules.allowConfig == qtrue);
	ASSERT_TRUE(rules.allowSaves == qtrue);
	ASSERT_TRUE(rules.allowExecutables == qfalse);
	ASSERT_TRUE(rules.numAllowedPaths == 0);
	
	FS_Sandbox_InitModRules(&rules);
	ASSERT_TRUE(rules.allowConfig == qfalse);
	ASSERT_TRUE(rules.allowSaves == qfalse);
	ASSERT_TRUE(rules.allowExecutables == qfalse);
	ASSERT_TRUE(rules.numAllowedPaths == 3);
}

TEST(sandbox_executable_check) {
	ASSERT_TRUE(FS_Sandbox_IsExecutable("test.exe") == qtrue);
	ASSERT_TRUE(FS_Sandbox_IsExecutable("test.dll") == qtrue);
	ASSERT_TRUE(FS_Sandbox_IsExecutable("test.so") == qtrue);
	ASSERT_TRUE(FS_Sandbox_IsExecutable("test.txt") == qfalse);
	ASSERT_TRUE(FS_Sandbox_IsExecutable("test") == qfalse);
}

TEST(sandbox_path_check) {
	fsSandboxRules_t rules;
	FS_Sandbox_InitDefaultRules(&rules);
	
	// Default rules allow config and saves
	ASSERT_TRUE(FS_Sandbox_CheckPath("config/test.cfg", &rules) == qtrue);
	ASSERT_TRUE(FS_Sandbox_CheckPath("saves/test.save", &rules) == qtrue);
	ASSERT_TRUE(FS_Sandbox_CheckPath("test.exe", &rules) == qfalse);  // Executables denied
	
	// Mod rules are more restrictive
	FS_Sandbox_InitModRules(&rules);
	ASSERT_TRUE(FS_Sandbox_CheckPath("maps/test.bsp", &rules) == qtrue);
	ASSERT_TRUE(FS_Sandbox_CheckPath("textures/test.tga", &rules) == qtrue);
	ASSERT_TRUE(FS_Sandbox_CheckPath("config/test.cfg", &rules) == qfalse);  // Config denied
	ASSERT_TRUE(FS_Sandbox_CheckPath("saves/test.save", &rules) == qfalse);  // Saves denied
}

TEST(write_policy_set_base_path) {
	FS_MountTable_Init();
	
	ASSERT_TRUE(FS_WritePolicy_SetBasePath("/tmp/write") == qtrue);
	// Note: Can't easily test internal state without exposing it
	
	FS_MountTable_Shutdown();
}

TEST(mount_duplicate_prevention) {
	FS_MountTable_Init();
	
	fsMount_t *mount1 = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	fsMount_t *mount2 = FS_Mount_Create("test/mount", FS_MOUNT_DIR, FS_PRIORITY_GAME);
	
	ASSERT_TRUE(FS_Mount_Add(mount1) == qtrue);
	ASSERT_TRUE(FS_Mount_Add(mount2) == qfalse);  // Duplicate should fail
	
	FS_Mount_Destroy(mount2);
	FS_Mount_Remove("test/mount");
	FS_Mount_Destroy(mount1);
	FS_MountTable_Shutdown();
}

TEST(mount_invalid_priority) {
	FS_MountTable_Init();
	
	// Invalid priorities should return NULL
	fsMount_t *mount1 = FS_Mount_Create("test/low", FS_MOUNT_DIR, FS_PRIORITY_FALLBACK - 1);
	fsMount_t *mount2 = FS_Mount_Create("test/high", FS_MOUNT_DIR, FS_PRIORITY_SYSTEM + 1);
	
	ASSERT_TRUE(mount1 == NULL);
	ASSERT_TRUE(mount2 == NULL);
	
	FS_MountTable_Shutdown();
}

// Main function to run all tests
int main(void) {
	RUN_TEST(mount_table_init_and_shutdown);
	RUN_TEST(mount_create_and_destroy);
	RUN_TEST(mount_add_and_remove);
	RUN_TEST(mount_priority_ordering);
	RUN_TEST(mount_enable_disable);
	RUN_TEST(write_policy_check);
	RUN_TEST(sandbox_rules);
	RUN_TEST(sandbox_executable_check);
	RUN_TEST(sandbox_path_check);
	RUN_TEST(write_policy_set_base_path);
	RUN_TEST(mount_duplicate_prevention);
	RUN_TEST(mount_invalid_priority);
	
	PRINT_TEST_SUMMARY();
	return (test_failed == 0) ? 0 : 1;
}
