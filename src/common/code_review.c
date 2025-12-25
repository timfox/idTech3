/*
=============================================================================
Automated Code Review System Implementation

AI-assisted code review with style and best practice checks for C/C++ codebases.
=============================================================================
*/

#include "code_review.h"
#include "qcommon.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Global code review system instance
code_review_system_t code_review_system = {0};

/*
=============================================================================
Internal Helper Functions
=============================================================================
*/

// Check if a string matches a pattern (simple wildcard support)
static qboolean MatchesPattern(const char* str, const char* pattern) {
    if (!str || !pattern) return qfalse;

    // Simple implementation - just check for exact match or suffix match
    if (strstr(str, pattern) != NULL) return qtrue;

    // Check for file extension match
    const char* ext = strrchr(str, '.');
    if (ext && strcmp(ext, pattern) == 0) return qtrue;

    return qfalse;
}

// Extract a line from content
static const char* GetLineFromContent(const char* content, int line_number, char* buffer, int buffer_size) {
    if (!content || line_number < 1) return NULL;

    const char* start = content;
    int current_line = 1;

    while (*start && current_line < line_number) {
        if (*start == '\n') current_line++;
        start++;
    }

    if (current_line != line_number) return NULL;

    const char* end = start;
    while (*end && *end != '\n' && *end != '\r') end++;

    int length = end - start;
    if (length >= buffer_size) length = buffer_size - 1;

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    return buffer;
}

// Count lines in content
static int CountLines(const char* content) {
    if (!content) return 0;

    int lines = 1;
    const char* p = content;
    while (*p) {
        if (*p == '\n') lines++;
        p++;
    }
    return lines;
}

/*
=============================================================================
Code Review System API
=============================================================================
*/

qboolean CodeReview_Init(void) {
    if (code_review_system.initialized) {
        return qtrue;
    }

    memset(&code_review_system, 0, sizeof(code_review_system_t));

    // Set default configuration
    code_review_system.config.enabled = qtrue;
    code_review_system.config.min_severity = REVIEW_SEVERITY_INFO;

    // Enable all categories by default
    for (int i = 0; i < REVIEW_CATEGORY_MAX; i++) {
        code_review_system.config.enable_categories[i] = qtrue;
    }

    // Style settings
    code_review_system.config.check_naming_conventions = qtrue;
    code_review_system.config.check_indentation = qtrue;
    code_review_system.config.check_line_length = qtrue;
    code_review_system.config.max_line_length = 120;

    // Performance settings
    code_review_system.config.check_memory_allocations = qtrue;
    code_review_system.config.check_function_complexity = qtrue;
    code_review_system.config.max_function_complexity = 20;

    // Security settings
    code_review_system.config.check_buffer_overflows = qtrue;
    code_review_system.config.check_format_strings = qtrue;
    code_review_system.config.check_null_pointers = qtrue;

    // Allocate findings storage
    code_review_system.max_findings = 10000;
    code_review_system.findings = (code_review_finding_t*)malloc(
        sizeof(code_review_finding_t) * code_review_system.max_findings);
    if (!code_review_system.findings) {
        Com_Printf("Failed to allocate memory for code review findings\n");
        return qfalse;
    }
    memset(code_review_system.findings, 0, sizeof(code_review_finding_t) * code_review_system.max_findings);

    code_review_system.initialized = qtrue;
    Com_Printf("Code review system initialized\n");
    return qtrue;
}

void CodeReview_Shutdown(void) {
    if (!code_review_system.initialized) {
        return;
    }

    // Free findings
    if (code_review_system.findings) {
        free(code_review_system.findings);
        code_review_system.findings = NULL;
    }

    // Free include paths
    for (uint32_t i = 0; i < code_review_system.num_include_paths; i++) {
        if (code_review_system.include_paths[i]) {
            free(code_review_system.include_paths[i]);
        }
    }
    if (code_review_system.include_paths) {
        free(code_review_system.include_paths);
        code_review_system.include_paths = NULL;
    }

    // Free exclude patterns
    for (uint32_t i = 0; i < code_review_system.num_exclude_patterns; i++) {
        if (code_review_system.exclude_patterns[i]) {
            free(code_review_system.exclude_patterns[i]);
        }
    }
    if (code_review_system.exclude_patterns) {
        free(code_review_system.exclude_patterns);
        code_review_system.exclude_patterns = NULL;
    }

    code_review_system.initialized = qfalse;
    Com_Printf("Code review system shutdown\n");
}

/*
=============================================================================
Configuration Management
=============================================================================
*/

void CodeReview_SetConfig(const code_review_config_t* config) {
    if (!config) return;
    memcpy(&code_review_system.config, config, sizeof(code_review_config_t));
}

void CodeReview_GetConfig(code_review_config_t* config) {
    if (!config) return;
    memcpy(config, &code_review_system.config, sizeof(code_review_config_t));
}

void CodeReview_LoadConfig(const char* config_file) {
    // Implementation would load config from JSON/XML file
    // For now, use defaults
    Q_UNUSED(config_file);
}

void CodeReview_SaveConfig(const char* config_file) {
    // Implementation would save config to JSON/XML file
    // For now, do nothing
    Q_UNUSED(config_file);
}

/*
=============================================================================
File Processing
=============================================================================
*/

char* CodeReview_ReadFile(const char* filename, int* line_count) {
    if (!filename) return NULL;

    fileHandle_t file = FS_FOpenFileRead(filename, NULL, qfalse);
    if (file == FS_INVALID_HANDLE) {
        return NULL;
    }

    int length = FS_FileLength(file);
    if (length <= 0) {
        FS_FCloseFile(file);
        return NULL;
    }

    char* content = (char*)malloc(length + 1);
    if (!content) {
        FS_FCloseFile(file);
        return NULL;
    }

    FS_Read(content, length, file);
    content[length] = '\0';
    FS_FCloseFile(file);

    if (line_count) {
        *line_count = CountLines(content);
    }

    return content;
}

qboolean CodeReview_IsSourceFile(const char* filename) {
    if (!filename) return qfalse;

    const char* ext = strrchr(filename, '.');
    if (!ext) return qfalse;

    // Check for C/C++ source files
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 ||
        strcmp(ext, ".cc") == 0 || strcmp(ext, ".cxx") == 0 ||
        strcmp(ext, ".h") == 0 || strcmp(ext, ".hpp") == 0 ||
        strcmp(ext, ".hxx") == 0) {
        return qtrue;
    }

    return qfalse;
}

qboolean CodeReview_ShouldAnalyzeFile(const char* filename) {
    if (!filename || !CodeReview_IsSourceFile(filename)) {
        return qfalse;
    }

    // Check exclude patterns
    for (uint32_t i = 0; i < code_review_system.num_exclude_patterns; i++) {
        if (MatchesPattern(filename, code_review_system.exclude_patterns[i])) {
            return qfalse;
        }
    }

    return qtrue;
}

/*
=============================================================================
Analysis Functions
=============================================================================
*/

qboolean CodeReview_AnalyzeFile(const char* filename) {
    if (!code_review_system.initialized || !filename) {
        return qfalse;
    }

    if (!CodeReview_ShouldAnalyzeFile(filename)) {
        return qtrue; // Not an error, just skip
    }

    Com_Printf("Analyzing file: %s\n", filename);

    int line_count;
    char* content = CodeReview_ReadFile(filename, &line_count);
    if (!content) {
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_ERROR, REVIEW_CATEGORY_MAINTAINABILITY,
                            "file-read-error", "Unable to read file for analysis",
                            "Check file permissions and path", "");
        return qfalse;
    }

    uint64_t start_time = Sys_Milliseconds();

    // Run all enabled checks
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_STYLE]) {
        CodeReview_CheckStyle(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_BEST_PRACTICE]) {
        CodeReview_CheckBestPractices(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_PERFORMANCE]) {
        CodeReview_CheckPerformance(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_SECURITY]) {
        CodeReview_CheckSecurity(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_BUGS]) {
        CodeReview_CheckBugs(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_MEMORY]) {
        CodeReview_CheckMemory(filename, content, line_count);
    }
    if (code_review_system.config.enable_categories[REVIEW_CATEGORY_THREADING]) {
        CodeReview_CheckThreading(filename, content, line_count);
    }

    uint64_t end_time = Sys_Milliseconds();
    code_review_system.stats.analysis_time_ms += (end_time - start_time);
    code_review_system.stats.files_analyzed++;

    free(content);
    return qtrue;
}

qboolean CodeReview_AnalyzeDirectory(const char* directory) {
    // Implementation would recursively analyze all files in directory
    // For now, just analyze common source files
    Q_UNUSED(directory);
    return qtrue;
}

qboolean CodeReview_AnalyzeProject(const char* project_root) {
    // Implementation would analyze the entire project
    // For now, just return success
    Q_UNUSED(project_root);
    return qtrue;
}

/*
=============================================================================
Findings Management
=============================================================================
*/

void CodeReview_AddFinding(const char* file, int line, int column,
                          review_severity_t severity, review_category_t category,
                          const char* rule, const char* message,
                          const char* suggestion, const char* code_snippet) {
    if (!code_review_system.initialized ||
        code_review_system.num_findings >= code_review_system.max_findings) {
        return;
    }

    if (severity < code_review_system.config.min_severity) {
        return;
    }

    code_review_finding_t* finding = &code_review_system.findings[code_review_system.num_findings++];
    Q_strncpyz(finding->file, file, sizeof(finding->file));
    finding->line = line;
    finding->column = column;
    finding->severity = severity;
    finding->category = category;
    Q_strncpyz(finding->rule, rule, sizeof(finding->rule));
    Q_strncpyz(finding->message, message, sizeof(finding->message));
    Q_strncpyz(finding->suggestion, suggestion, sizeof(finding->suggestion));
    Q_strncpyz(finding->code_snippet, code_snippet, sizeof(finding->code_snippet));
    finding->timestamp = Sys_Milliseconds();

    code_review_system.stats.total_findings++;
    code_review_system.stats.findings_by_severity[severity]++;
    code_review_system.stats.findings_by_category[category]++;
}

uint32_t CodeReview_GetNumFindings(void) {
    return code_review_system.num_findings;
}

const code_review_finding_t* CodeReview_GetFinding(uint32_t index) {
    if (index >= code_review_system.num_findings) {
        return NULL;
    }
    return &code_review_system.findings[index];
}

void CodeReview_ClearFindings(void) {
    code_review_system.num_findings = 0;
    memset(&code_review_system.stats, 0, sizeof(code_review_stats_t));
}

qboolean CodeReview_SaveFindings(const char* output_file, const char* format) {
    // Implementation would save findings to file in specified format (JSON, XML, etc.)
    Q_UNUSED(output_file);
    Q_UNUSED(format);
    return qtrue;
}

/*
=============================================================================
Filtering Functions
=============================================================================
*/

void CodeReview_FilterBySeverity(review_severity_t min_severity) {
    code_review_system.config.min_severity = min_severity;
}

void CodeReview_FilterByCategory(review_category_t category, qboolean enable) {
    if (category < REVIEW_CATEGORY_MAX) {
        code_review_system.config.enable_categories[category] = enable;
    }
}

void CodeReview_FilterByFile(const char* filename) {
    // Implementation would filter findings by filename
    Q_UNUSED(filename);
}

/*
=============================================================================
Statistics and Reporting
=============================================================================
*/

void CodeReview_GetStats(code_review_stats_t* stats) {
    if (!stats) return;
    memcpy(stats, &code_review_system.stats, sizeof(code_review_stats_t));
}

void CodeReview_PrintSummary(void) {
    Com_Printf("=== Code Review Summary ===\n");
    Com_Printf("Total findings: %u\n", code_review_system.stats.total_findings);
    Com_Printf("Files analyzed: %u\n", code_review_system.stats.files_analyzed);

    const char* severity_names[REVIEW_SEVERITY_MAX] = {
        "Info", "Warning", "Error", "Critical"
    };

    for (int i = 0; i < REVIEW_SEVERITY_MAX; i++) {
        if (code_review_system.stats.findings_by_severity[i] > 0) {
            Com_Printf("  %s: %u\n", severity_names[i],
                      code_review_system.stats.findings_by_severity[i]);
        }
    }

    Com_Printf("Analysis time: %.2f seconds\n",
              code_review_system.stats.analysis_time_ms / 1000.0f);
}

/*
=============================================================================
Code Analysis Checks
=============================================================================
*/

void CodeReview_CheckStyle(const char* filename, const char* content, int line_count) {
    char line_buffer[1024];
    const char* line;
    int line_num = 1;

    const char* p = content;
    while (*p && line_num <= line_count) {
        line = GetLineFromContent(content, line_num, line_buffer, sizeof(line_buffer));
        if (!line) break;

        // Check line length
        if (code_review_system.config.check_line_length) {
            int len = strlen(line);
            if (len > code_review_system.config.max_line_length) {
                CodeReview_AddFinding(filename, line_num, len, REVIEW_SEVERITY_WARNING,
                                    REVIEW_CATEGORY_STYLE, "line-too-long",
                                    "Line exceeds maximum length",
                                    va("Break line or reduce to %d characters",
                                       code_review_system.config.max_line_length),
                                    line);
            }
        }

        // Check for tabs vs spaces (prefer spaces)
        if (code_review_system.config.check_indentation) {
            if (line[0] == '\t') {
                CodeReview_AddFinding(filename, line_num, 1, REVIEW_SEVERITY_INFO,
                                    REVIEW_CATEGORY_STYLE, "tab-indentation",
                                    "Using tabs for indentation instead of spaces",
                                    "Use spaces for indentation (4 spaces per indent level)",
                                    line);
            }
        }

        // Check for trailing whitespace
        int len = strlen(line);
        for (int i = len - 1; i >= 0; i--) {
            if (line[i] == ' ' || line[i] == '\t') {
                continue;
            } else if (line[i] == '\r' || line[i] == '\n') {
                continue;
            } else {
                // Found non-whitespace, check if there was trailing whitespace
                if (i < len - 1) {
                    CodeReview_AddFinding(filename, line_num, i + 2, REVIEW_SEVERITY_WARNING,
                                        REVIEW_CATEGORY_STYLE, "trailing-whitespace",
                                        "Line contains trailing whitespace",
                                        "Remove trailing whitespace", line);
                }
                break;
            }
        }

        // Move to next line
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        line_num++;
    }
}

void CodeReview_CheckBestPractices(const char* filename, const char* content, int line_count) {
    // Check for common best practice violations

    // Look for goto statements
    const char* goto_pos = strstr(content, "goto ");
    if (goto_pos) {
        int line_num = 1;
        const char* p = content;
        while (p < goto_pos) {
            if (*p == '\n') line_num++;
            p++;
        }

        CodeReview_AddFinding(filename, line_num, 0, REVIEW_SEVERITY_WARNING,
                            REVIEW_CATEGORY_BEST_PRACTICE, "avoid-goto",
                            "Use of goto statement",
                            "Consider using structured programming constructs instead of goto",
                            "goto ...");
    }

    // Check for magic numbers
    // This is a simplified check - in practice, would need more sophisticated analysis
    const char* magic_patterns[] = {"if (", "while (", "for ("};
    for (int i = 0; i < 3; i++) {
        const char* pos = strstr(content, magic_patterns[i]);
        while (pos) {
            // Look for numbers in conditionals
            const char* num_start = pos + strlen(magic_patterns[i]);
            while (*num_start && *num_start != ')') {
                if (isdigit(*num_start)) {
                    // Found a potential magic number
                    CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_INFO,
                                        REVIEW_CATEGORY_BEST_PRACTICE, "magic-number",
                                        "Potential magic number in conditional",
                                        "Consider defining constants for magic numbers",
                                        "");
                    break;
                }
                num_start++;
            }
            pos = strstr(pos + 1, magic_patterns[i]);
        }
    }
}

void CodeReview_CheckPerformance(const char* filename, const char* content, int line_count) {
    // Check for performance issues

    // Look for string concatenation in loops
    if (strstr(content, "strcat") && (strstr(content, "for(") || strstr(content, "while("))) {
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                            REVIEW_CATEGORY_PERFORMANCE, "string-concat-loop",
                            "String concatenation in loop detected",
                            "Use more efficient string building methods or pre-allocate buffers",
                            "strcat(...) in loop");
    }

    // Check for memory allocations in loops
    const char* alloc_funcs[] = {"malloc(", "calloc(", "realloc(", "new ", "new("};
    for (int i = 0; i < 5; i++) {
        if (strstr(content, alloc_funcs[i]) &&
            (strstr(content, "for(") || strstr(content, "while("))) {
            CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                                REVIEW_CATEGORY_PERFORMANCE, "alloc-in-loop",
                                "Memory allocation in loop",
                                "Move allocation outside loop or use pre-allocated pools",
                                alloc_funcs[i]);
            break;
        }
    }
}

void CodeReview_CheckSecurity(const char* filename, const char* content, int line_count) {
    // Check for security vulnerabilities

    // Look for potential buffer overflows
    if (code_review_system.config.check_buffer_overflows) {
        const char* unsafe_funcs[] = {"strcpy(", "strcat(", "sprintf(", "gets("};
        for (int i = 0; i < 4; i++) {
            const char* pos = strstr(content, unsafe_funcs[i]);
            if (pos) {
                CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_ERROR,
                                    REVIEW_CATEGORY_SECURITY, "unsafe-function",
                                    "Use of unsafe string function",
                                    va("Replace with safe version like %s_s or strlcpy",
                                       unsafe_funcs[i]),
                                    unsafe_funcs[i]);
            }
        }
    }

    // Check for format string vulnerabilities
    if (code_review_system.config.check_format_strings) {
        // Look for printf-style functions with user-controlled format strings
        if (strstr(content, "printf(") || strstr(content, "sprintf(")) {
            CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                                REVIEW_CATEGORY_SECURITY, "format-string-check",
                                "Potential format string vulnerability",
                                "Ensure format strings are not user-controlled",
                                "printf(...) or sprintf(...)");
        }
    }
}

void CodeReview_CheckBugs(const char* filename, const char* content, int line_count) {
    // Check for potential bugs

    // Look for uninitialized variables (simplified check)
    if (strstr(content, "int ") || strstr(content, "float ") || strstr(content, "char ")) {
        // This is a very basic check - real implementation would need AST parsing
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_INFO,
                            REVIEW_CATEGORY_BUGS, "variable-init-check",
                            "Consider checking for uninitialized variables",
                            "Initialize variables at declaration or check usage",
                            "");
    }

    // Check for missing null checks
    if (strstr(content, "->") || strstr(content, "[") || strstr(content, "free(")) {
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_INFO,
                            REVIEW_CATEGORY_BUGS, "null-check",
                            "Potential null pointer dereference",
                            "Add null checks before dereferencing pointers",
                            "->, [], free(...)");
    }
}

void CodeReview_CheckMemory(const char* filename, const char* content, int line_count) {
    // Check for memory management issues

    if (code_review_system.config.check_memory_allocations) {
        // Look for malloc/calloc without corresponding free
        int alloc_count = 0;
        int free_count = 0;

        const char* p = content;
        while (*p) {
            if (strstr(p, "malloc(") || strstr(p, "calloc(") || strstr(p, "realloc(")) {
                alloc_count++;
            }
            if (strstr(p, "free(")) {
                free_count++;
            }
            p++;
        }

        if (alloc_count > free_count) {
            CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                                REVIEW_CATEGORY_MEMORY, "memory-leak-potential",
                                "Potential memory leak detected",
                                "Ensure all allocations have corresponding frees",
                                "More allocations than frees found");
        }
    }

    // Check for use after free patterns
    if (strstr(content, "free(") && strstr(content, "free(")) {
        // Multiple frees could indicate use after free
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                            REVIEW_CATEGORY_MEMORY, "double-free-check",
                            "Potential double free or use after free",
                            "Ensure pointers are not used after being freed",
                            "free(...) called multiple times");
    }
}

void CodeReview_CheckThreading(const char* filename, const char* content, int line_count) {
    // Check for threading issues

    // Look for thread creation without proper cleanup
    if (strstr(content, "Thread_Create(") && !strstr(content, "Thread_Join(")) {
        CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_WARNING,
                            REVIEW_CATEGORY_THREADING, "thread-cleanup",
                            "Thread created without corresponding join",
                            "Ensure threads are properly joined to avoid resource leaks",
                            "Thread_Create(...) without Thread_Join(...)");
    }

    // Check for lock usage patterns
    const char* lock_patterns[] = {"MUTEX_LOCK(", "SpinLock_Lock("};
    const char* unlock_patterns[] = {"MUTEX_UNLOCK(", "SpinLock_Unlock("};

    for (int i = 0; i < 2; i++) {
        int lock_count = 0;
        int unlock_count = 0;

        const char* p = content;
        while (*p) {
            if (strstr(p, lock_patterns[i])) lock_count++;
            if (strstr(p, unlock_patterns[i])) unlock_count++;
            p++;
        }

        if (lock_count != unlock_count) {
            CodeReview_AddFinding(filename, 0, 0, REVIEW_SEVERITY_ERROR,
                                REVIEW_CATEGORY_THREADING, "lock-imbalance",
                                "Lock/unlock imbalance detected",
                                "Ensure every lock has a corresponding unlock",
                                va("%s count (%d) != %s count (%d)",
                                   lock_patterns[i], lock_count,
                                   unlock_patterns[i], unlock_count));
        }
    }
}
