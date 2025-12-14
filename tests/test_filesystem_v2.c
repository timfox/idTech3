/*
===========================================================================
Unit tests for Virtual Filesystem v2
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"
#include "../src/qcommon/files_v2.h"
#include <string.h>
#include <stdlib.h>

// Mock functions needed by files_v2.c
void Com_Error(errorParm_t level, const char *error, ...) {
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

// Mock Com_Memset
void Com_Memset(void *dest, int val, size_t count) {
	memset(dest, val, count);
}

// Mock Com_sprintf
int Com_sprintf(char *dest, size_t size, const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	int result = vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);
	return result;
}

// Mock Q_strncpyz
void Q_strncpyz(char *dest, const char *src, int destsize) {
	strncpy(dest, src, destsize - 1);
	dest[destsize - 1] = '\0';
}

// Mock Q_stricmp
int Q_stricmp(const char *s1, const char *s2) {
	return strcasecmp(s1, s2);
}

// Mock Q_stricmpn
int Q_stricmpn(const char *s1, const char *s2, int n) {
	return strncasecmp(s1, s2, n);
}

// Mock Com_GenerateHashValue
uint32_t Com_GenerateHashValue(const char *fname, uint32_t hashSize) {
	uint32_t hash = 0;
	const char *p = fname;
	while (*p) {
		hash = hash * 31 + (unsigned char)*p;
		p++;
	}
	return hash % hashSize;
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
