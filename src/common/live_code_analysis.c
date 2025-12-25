/*
=============================================================================
Live Code Analysis System Implementation

Real-time feedback during development with IDE integration and incremental analysis.
=============================================================================
*/

#include "live_code_analysis.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>

// Global live code analysis system instance
live_code_analysis_system_t live_code_analysis_system = {0};

/*
=============================================================================
Internal Helper Functions
=============================================================================
*/

// Hash function for session lookup
static uint32_t SessionHash(const char* filename) {
    uint32_t hash = 0;
    const char* p = filename;
    while (*p) {
        hash = hash * 31 + *p;
        p++;
    }
    return hash % 256; // Simple hash for session lookup
}

// Find session by filename
static live_session_t* FindSession(const char* filename) {
    for (uint32_t i = 0; i < live_code_analysis_system.num_sessions; i++) {
        if (strcmp(live_code_analysis_system.sessions[i].filename, filename) == 0) {
            return &live_code_analysis_system.sessions[i];
        }
    }
    return NULL;
}

// Create new session
static live_session_t* CreateSession(const char* filename) {
    if (live_code_analysis_system.num_sessions >= live_code_analysis_system.max_sessions) {
        return NULL; // No more sessions available
    }

    live_session_t* session = &live_code_analysis_system.sessions[live_code_analysis_system.num_sessions++];
    memset(session, 0, sizeof(live_session_t));

    Q_strncpyz(session->filename, filename, sizeof(session->filename));
    session->version = 1;
    session->is_open = qtrue;
    session->max_findings = 100;
    session->findings = (live_finding_t*)malloc(sizeof(live_finding_t) * session->max_findings);

    if (!session->findings) {
        live_code_analysis_system.num_sessions--;
        return NULL;
    }

    memset(session->findings, 0, sizeof(live_finding_t) * session->max_findings);

    return session;
}

// Destroy session
static void DestroySession(live_session_t* session) {
    if (session->content) {
        free(session->content);
        session->content = NULL;
    }

    if (session->previous_content) {
        free(session->previous_content);
        session->previous_content = NULL;
    }

    if (session->changed_lines) {
        free(session->changed_lines);
        session->changed_lines = NULL;
    }

    if (session->findings) {
        free(session->findings);
        session->findings = NULL;
    }

    memset(session, 0, sizeof(live_session_t));
}

// Compare two content strings and find changed lines
static void FindChangedLines(const char* old_content, const char* new_content,
                           int** changed_lines, uint32_t* num_changed) {
    if (!old_content || !new_content) return;

    // Simple line-by-line comparison
    // In a real implementation, this would use diff algorithms
    const char* old_line = old_content;
    const char* new_line = new_content;
    int line_num = 1;
    int* temp_lines = NULL;
    uint32_t temp_count = 0;
    uint32_t temp_capacity = 16;

    temp_lines = (int*)malloc(sizeof(int) * temp_capacity);

    while (*old_line && *new_line) {
        const char* old_end = strchr(old_line, '\n');
        const char* new_end = strchr(new_line, '\n');

        if (!old_end) old_end = old_line + strlen(old_line);
        if (!new_end) new_end = new_line + strlen(new_line);

        size_t old_len = old_end - old_line;
        size_t new_len = new_end - new_line;

        // Compare lines
        if (old_len != new_len || strncmp(old_line, new_line, old_len) != 0) {
            // Line changed
            if (temp_count >= temp_capacity) {
                temp_capacity *= 2;
                temp_lines = (int*)realloc(temp_lines, sizeof(int) * temp_capacity);
            }
            temp_lines[temp_count++] = line_num;
        }

        line_num++;
        old_line = old_end + (*old_end ? 1 : 0);
        new_line = new_end + (*new_end ? 1 : 0);
    }

    *changed_lines = temp_lines;
    *num_changed = temp_count;
}

// Convert code review finding to live finding
static void ConvertToLiveFinding(const code_review_finding_t* base, live_finding_t* live) {
    memcpy(&live->base, base, sizeof(code_review_finding_t));
    live->timestamp = Sys_Milliseconds() * 1000000ULL;
    live->acknowledged = qfalse;
    live->suppressed = qfalse;
    live->suppression_reason[0] = '\0';

    // Set IDE-specific data based on severity
    switch (base->severity) {
        case REVIEW_SEVERITY_INFO:
            Q_strncpyz(live->ide_marker, "info", sizeof(live->ide_marker));
            live->ide_severity = 3;
            break;
        case REVIEW_SEVERITY_WARNING:
            Q_strncpyz(live->ide_marker, "warning", sizeof(live->ide_marker));
            live->ide_severity = 2;
            break;
        case REVIEW_SEVERITY_ERROR:
            Q_strncpyz(live->ide_marker, "error", sizeof(live->ide_marker));
            live->ide_severity = 1;
            break;
        case REVIEW_SEVERITY_CRITICAL:
            Q_strncpyz(live->ide_marker, "error", sizeof(live->ide_marker));
            live->ide_severity = 1;
            break;
    }

    // Generate quick fix suggestions based on rule
    if (strstr(base->rule, "line-too-long")) {
        Com_sprintf(live->quick_fix, sizeof(live->quick_fix),
                   "Break line into multiple lines or reduce length to %d characters",
                   120); // Default max length
    } else if (strstr(base->rule, "trailing-whitespace")) {
        Q_strncpyz(live->quick_fix, "Remove trailing whitespace", sizeof(live->quick_fix));
    } else if (strstr(base->rule, "tab-indentation")) {
        Q_strncpyz(live->quick_fix, "Replace tabs with spaces", sizeof(live->quick_fix));
    } else {
        Q_strncpyz(live->quick_fix, base->suggestion, sizeof(live->quick_fix));
    }
}

/*
=============================================================================
Analysis Worker Thread
=============================================================================
*/

// Work item for analysis queue
typedef struct {
    live_event_type_t event_type;
    char filename[MAX_OSPATH];
    int line;
    int column;
    char* data;
    int data_length;
} analysis_work_item_t;

// Analysis worker thread
static THREAD_RETURN THREAD_CALL AnalysisWorker(void* arg) {
    Q_UNUSED(arg);

    while (!live_code_analysis_system.should_exit) {
        // Wait for work
        MUTEX_LOCK(live_code_analysis_system.work_mutex);
        while (atomic_load_explicit(&live_code_analysis_system.work_available_count, memory_order_relaxed) == 0 &&
               !live_code_analysis_system.should_exit) {
            CONDITION_TIMED_WAIT(live_code_analysis_system.work_available,
                               live_code_analysis_system.work_mutex, 100); // 100ms timeout
        }
        MUTEX_UNLOCK(live_code_analysis_system.work_mutex);

        if (live_code_analysis_system.should_exit) break;

        // Process work items
        while (atomic_load_explicit(&live_code_analysis_system.work_available_count, memory_order_relaxed) > 0) {
            uint32_t head = atomic_load_explicit(&live_code_analysis_system.work_queue_head, memory_order_relaxed);
            analysis_work_item_t* work = (analysis_work_item_t*)live_code_analysis_system.work_queue[head];

            if (work) {
                // Process the work item
                switch (work->event_type) {
                    case LIVE_EVENT_FILE_CHANGE:
                        LiveCodeAnalysis_UpdateContent(work->filename, work->data, work->data_length);
                        LiveCodeAnalysis_AnalyzeNow(work->filename);
                        break;

                    case LIVE_EVENT_FILE_OPEN:
                        LiveCodeAnalysis_OpenFile(work->filename);
                        break;

                    case LIVE_EVENT_FILE_CLOSE:
                        LiveCodeAnalysis_CloseFile(work->filename);
                        break;

                    case LIVE_EVENT_FILE_SAVE:
                        LiveCodeAnalysis_AnalyzeNow(work->filename);
                        break;

                    default:
                        break;
                }

                // Free work item
                if (work->data) free(work->data);
                free(work);
            }

            atomic_store_explicit(&live_code_analysis_system.work_queue_head,
                                (head + 1) % live_code_analysis_system.work_queue_size, memory_order_relaxed);
            atomic_fetch_sub_explicit(&live_code_analysis_system.work_available_count, 1, memory_order_relaxed);
        }

        // Small delay to prevent busy waiting
        Thread_Sleep(10);
    }

    return 0;
}

/*
=============================================================================
Live Code Analysis API
=============================================================================
*/

qboolean LiveCodeAnalysis_Init(void) {
    if (live_code_analysis_system.initialized) {
        return qtrue;
    }

    memset(&live_code_analysis_system, 0, sizeof(live_code_analysis_system_t));

    // Set default configuration
    live_code_analysis_system.config.mode = LIVE_MODE_BACKGROUND;
    live_code_analysis_system.config.analysis_interval_ms = 1000; // 1 second
    live_code_analysis_system.config.max_file_size_kb = 1024; // 1MB
    live_code_analysis_system.config.max_findings_per_file = 100;
    live_code_analysis_system.config.analyze_on_type = qfalse;
    live_code_analysis_system.config.analyze_on_save = qtrue;
    live_code_analysis_system.config.typing_delay_ms = 500;
    live_code_analysis_system.config.use_incremental = qtrue;
    live_code_analysis_system.config.incremental_threshold = 10;
    live_code_analysis_system.config.show_acked_findings = qfalse;
    live_code_analysis_system.config.auto_acknowledge = qtrue;

    // Allocate sessions
    live_code_analysis_system.max_sessions = 64;
    live_code_analysis_system.sessions = (live_session_t*)malloc(
        sizeof(live_session_t) * live_code_analysis_system.max_sessions);

    if (!live_code_analysis_system.sessions) {
        Com_Printf("Failed to allocate memory for live analysis sessions\n");
        return qfalse;
    }

    memset(live_code_analysis_system.sessions, 0,
           sizeof(live_session_t) * live_code_analysis_system.max_sessions);

    // Allocate work queue
    live_code_analysis_system.work_queue_size = 256;
    live_code_analysis_system.work_queue = (void**)malloc(
        sizeof(void*) * live_code_analysis_system.work_queue_size);

    if (!live_code_analysis_system.work_queue) {
        free(live_code_analysis_system.sessions);
        Com_Printf("Failed to allocate memory for work queue\n");
        return qfalse;
    }

    memset(live_code_analysis_system.work_queue, 0,
           sizeof(void*) * live_code_analysis_system.work_queue_size);

    atomic_init(&live_code_analysis_system.work_available_count, 0);

    // Initialize synchronization
    MUTEX_INIT(live_code_analysis_system.work_mutex);
    CONDITION_INIT(live_code_analysis_system.work_available);

    // Start analysis thread
    if (!Thread_Create(&live_code_analysis_system.analysis_thread, AnalysisWorker,
                      NULL, "LiveAnalysis", THREAD_PRIORITY_LOW)) {
        MUTEX_DESTROY(live_code_analysis_system.work_mutex);
        CONDITION_DESTROY(live_code_analysis_system.work_available);
        free(live_code_analysis_system.work_queue);
        free(live_code_analysis_system.sessions);
        Com_Printf("Failed to create analysis thread\n");
        return qfalse;
    }

    live_code_analysis_system.initialized = qtrue;
    Com_Printf("Live code analysis system initialized\n");
    return qtrue;
}

void LiveCodeAnalysis_Shutdown(void) {
    if (!live_code_analysis_system.initialized) {
        return;
    }

    // Signal thread to exit
    live_code_analysis_system.should_exit = qtrue;
    MUTEX_LOCK(live_code_analysis_system.work_mutex);
    CONDITION_SIGNAL(live_code_analysis_system.work_available);
    MUTEX_UNLOCK(live_code_analysis_system.work_mutex);

    // Wait for thread to finish
    Thread_Join(live_code_analysis_system.analysis_thread);

    // Clean up sessions
    for (uint32_t i = 0; i < live_code_analysis_system.num_sessions; i++) {
        DestroySession(&live_code_analysis_system.sessions[i]);
    }

    if (live_code_analysis_system.sessions) {
        free(live_code_analysis_system.sessions);
        live_code_analysis_system.sessions = NULL;
    }

    if (live_code_analysis_system.work_queue) {
        // Free any remaining work items
        for (uint32_t i = 0; i < live_code_analysis_system.work_queue_size; i++) {
            if (live_code_analysis_system.work_queue[i]) {
                analysis_work_item_t* work = (analysis_work_item_t*)live_code_analysis_system.work_queue[i];
                if (work->data) free(work->data);
                free(work);
            }
        }
        free(live_code_analysis_system.work_queue);
        live_code_analysis_system.work_queue = NULL;
    }

    // Clean up synchronization
    MUTEX_DESTROY(live_code_analysis_system.work_mutex);
    CONDITION_DESTROY(live_code_analysis_system.work_available);

    live_code_analysis_system.initialized = qfalse;
    Com_Printf("Live code analysis system shutdown\n");
}

qboolean LiveCodeAnalysis_SetMode(live_analysis_mode_t mode) {
    if (mode >= LIVE_MODE_INCREMENTAL + 1) return qfalse;
    live_code_analysis_system.config.mode = mode;
    return qtrue;
}

live_analysis_mode_t LiveCodeAnalysis_GetMode(void) {
    return live_code_analysis_system.config.mode;
}

qboolean LiveCodeAnalysis_SetIDEProtocol(ide_protocol_t protocol) {
    if (protocol >= IDE_PROTOCOL_EMACS + 1) return qfalse;
    live_code_analysis_system.config.ide.protocol = protocol;
    return qtrue;
}

ide_protocol_t LiveCodeAnalysis_GetIDEProtocol(void) {
    return live_code_analysis_system.config.ide.protocol;
}

/*
=============================================================================
Session Management
=============================================================================
*/

live_session_t* LiveCodeAnalysis_OpenFile(const char* filename) {
    if (!live_code_analysis_system.initialized || !filename) {
        return NULL;
    }

    // Check if session already exists
    live_session_t* session = FindSession(filename);
    if (session) {
        session->is_open = qtrue;
        return session;
    }

    // Create new session
    session = CreateSession(filename);
    if (!session) {
        return NULL;
    }

    // Load initial content
    char* content = CodeReview_ReadFile(filename, &session->content_length);
    if (content) {
        session->content = content;
        session->last_modified = Sys_Milliseconds();

        // Store as previous content for incremental analysis
        session->previous_content = (char*)malloc(session->content_length + 1);
        if (session->previous_content) {
            memcpy(session->previous_content, session->content, session->content_length + 1);
        }
    }

    Com_Printf("Opened file for live analysis: %s\n", filename);
    return session;
}

qboolean LiveCodeAnalysis_CloseFile(const char* filename) {
    if (!live_code_analysis_system.initialized || !filename) {
        return qfalse;
    }

    live_session_t* session = FindSession(filename);
    if (!session) {
        return qfalse;
    }

    session->is_open = qfalse;

    // Clear findings
    session->num_findings = 0;

    Com_Printf("Closed file for live analysis: %s\n", filename);
    return qtrue;
}

live_session_t* LiveCodeAnalysis_GetSession(const char* filename) {
    if (!live_code_analysis_system.initialized || !filename) {
        return NULL;
    }

    return FindSession(filename);
}

/*
=============================================================================
Content Updates and Analysis
=============================================================================
*/

qboolean LiveCodeAnalysis_UpdateContent(const char* filename, const char* content, int length) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session || !content) {
        return qfalse;
    }

    // Free old content
    if (session->content) {
        free(session->content);
    }

    // Allocate new content
    session->content = (char*)malloc(length + 1);
    if (!session->content) {
        return qfalse;
    }

    memcpy(session->content, content, length);
    session->content[length] = '\0';
    session->content_length = length;
    session->version++;
    session->last_modified = Sys_Milliseconds();

    return qtrue;
}

qboolean LiveCodeAnalysis_UpdateRegion(const char* filename, int start_line, int end_line, const char* new_content) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session) {
        return qfalse;
    }

    // For now, do a full content update
    // In a real implementation, this would do surgical updates
    return LiveCodeAnalysis_UpdateContent(filename, new_content, strlen(new_content));
}

qboolean LiveCodeAnalysis_AnalyzeNow(const char* filename) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session || !session->content) {
        return qfalse;
    }

    uint64_t start_time = Sys_Milliseconds();

    // Clear old findings
    session->num_findings = 0;

    // Run analysis using the static code review system
    CodeReview_ClearFindings();

    // Analyze the content
    int line_count = CountLines(session->content);
    CodeReview_CheckStyle(filename, session->content, line_count);
    CodeReview_CheckBestPractices(filename, session->content, line_count);
    CodeReview_CheckPerformance(filename, session->content, line_count);
    CodeReview_CheckSecurity(filename, session->content, line_count);
    CodeReview_CheckBugs(filename, session->content, line_count);
    CodeReview_CheckMemory(filename, session->content, line_count);
    CodeReview_CheckThreading(filename, session->content, line_count);

    // Convert findings to live findings
    uint32_t num_static_findings = CodeReview_GetNumFindings();
    for (uint32_t i = 0; i < num_static_findings && session->num_findings < session->max_findings; i++) {
        const code_review_finding_t* static_finding = CodeReview_GetFinding(i);
        if (!static_finding) continue;

        live_finding_t* live_finding = &session->findings[session->num_findings++];
        ConvertToLiveFinding(static_finding, live_finding);
        live_finding->version = session->version;
    }

    uint64_t end_time = Sys_Milliseconds();

    // Update statistics
    atomic_fetch_add_explicit(&live_code_analysis_system.total_analyses, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&live_code_analysis_system.total_findings, session->num_findings, memory_order_relaxed);
    atomic_fetch_add_explicit(&live_code_analysis_system.analysis_time_ms, end_time - start_time, memory_order_relaxed);

    // Send to IDE if configured
    if (live_code_analysis_system.config.ide.protocol != IDE_PROTOCOL_NONE) {
        LiveCodeAnalysis_SendToIDE(filename, session->findings, session->num_findings);
    }

    return qtrue;
}

qboolean LiveCodeAnalysis_AnalyzeIncremental(const char* filename, int start_line, int end_line) {
    // For now, just do a full analysis
    // In a real implementation, this would only analyze changed regions
    return LiveCodeAnalysis_AnalyzeNow(filename);
}

/*
=============================================================================
Event Handling
=============================================================================
*/

void LiveCodeAnalysis_OnFileEvent(live_event_type_t event, const char* filename,
                                 int line, int column, const char* data) {
    if (!live_code_analysis_system.initialized) {
        return;
    }

    // Queue work item for the analysis thread
    analysis_work_item_t* work = (analysis_work_item_t*)malloc(sizeof(analysis_work_item_t));
    if (!work) return;

    memset(work, 0, sizeof(analysis_work_item_t));
    work->event_type = event;
    Q_strncpyz(work->filename, filename, sizeof(work->filename));
    work->line = line;
    work->column = column;

    if (data) {
        work->data_length = strlen(data);
        work->data = (char*)malloc(work->data_length + 1);
        if (work->data) {
            memcpy(work->data, data, work->data_length + 1);
        }
    }

    // Add to work queue
    uint32_t tail = atomic_load_explicit(&live_code_analysis_system.work_queue_tail, memory_order_relaxed);
    if (atomic_load_explicit(&live_code_analysis_system.work_available_count, memory_order_relaxed) <
        live_code_analysis_system.work_queue_size) {

        live_code_analysis_system.work_queue[tail] = work;
        atomic_store_explicit(&live_code_analysis_system.work_queue_tail,
                            (tail + 1) % live_code_analysis_system.work_queue_size, memory_order_relaxed);
        atomic_fetch_add_explicit(&live_code_analysis_system.work_available_count, 1, memory_order_relaxed);

        // Signal analysis thread
        MUTEX_LOCK(live_code_analysis_system.work_mutex);
        CONDITION_SIGNAL(live_code_analysis_system.work_available);
        MUTEX_UNLOCK(live_code_analysis_system.work_mutex);
    } else {
        // Queue full, process immediately for critical events
        if (event == LIVE_EVENT_FILE_CHANGE) {
            LiveCodeAnalysis_UpdateContent(filename, data, work->data_length);
            LiveCodeAnalysis_AnalyzeNow(filename);
        }

        if (work->data) free(work->data);
        free(work);
    }
}

/*
=============================================================================
Finding Management
=============================================================================
*/

uint32_t LiveCodeAnalysis_GetFindings(const char* filename, live_finding_t** findings) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session || !findings) {
        return 0;
    }

    *findings = session->findings;
    return session->num_findings;
}

qboolean LiveCodeAnalysis_AcknowledgeFinding(const char* filename, int line, int column) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session) return qfalse;

    for (uint32_t i = 0; i < session->num_findings; i++) {
        live_finding_t* finding = &session->findings[i];
        if (finding->base.line == line && finding->base.column == column) {
            finding->acknowledged = qtrue;
            return qtrue;
        }
    }

    return qfalse;
}

qboolean LiveCodeAnalysis_SuppressFinding(const char* filename, int line, int column, const char* reason) {
    live_session_t* session = LiveCodeAnalysis_GetSession(filename);
    if (!session) return qfalse;

    for (uint32_t i = 0; i < session->num_findings; i++) {
        live_finding_t* finding = &session->findings[i];
        if (finding->base.line == line && finding->base.column == column) {
            finding->suppressed = qtrue;
            Q_strncpyz(finding->suppression_reason, reason, sizeof(finding->suppression_reason));
            return qtrue;
        }
    }

    return qfalse;
}

/*
=============================================================================
IDE Integration
=============================================================================
*/

void LiveCodeAnalysis_SendToIDE(const char* filename, const live_finding_t* findings, uint32_t count) {
    if (!live_code_analysis_system.config.ide.on_diagnostics_ready) {
        return;
    }

    // Filter findings based on configuration
    uint32_t filtered_count = 0;
    const live_finding_t* filtered_findings[1000]; // Reasonable limit

    for (uint32_t i = 0; i < count && filtered_count < 1000; i++) {
        const live_finding_t* finding = &findings[i];

        // Skip acknowledged findings if configured
        if (!live_code_analysis_system.config.show_acked_findings && finding->acknowledged) {
            continue;
        }

        // Skip suppressed findings
        if (finding->suppressed) {
            continue;
        }

        filtered_findings[filtered_count++] = finding;
    }

    // Send to IDE
    live_code_analysis_system.config.ide.on_diagnostics_ready(filename,
                                                             filtered_findings[0],
                                                             filtered_count);
}

void LiveCodeAnalysis_RequestCompletion(const char* filename, int line, int column) {
    // Placeholder for code completion functionality
    // In a real implementation, this would analyze the code context
    // and provide completion suggestions
    Q_UNUSED(filename);
    Q_UNUSED(line);
    Q_UNUSED(column);
}

/*
=============================================================================
Performance Monitoring
=============================================================================
*/

void LiveCodeAnalysis_GetStats(uint64_t* total_analyses, uint64_t* total_findings, uint64_t* analysis_time) {
    if (total_analyses) *total_analyses = atomic_load_explicit(&live_code_analysis_system.total_analyses, memory_order_relaxed);
    if (total_findings) *total_findings = atomic_load_explicit(&live_code_analysis_system.total_findings, memory_order_relaxed);
    if (analysis_time) *analysis_time = atomic_load_explicit(&live_code_analysis_system.analysis_time_ms, memory_order_relaxed);
}

/*
=============================================================================
Configuration
=============================================================================
*/

void LiveCodeAnalysis_SetConfig(const live_analysis_config_t* config) {
    if (!config) return;
    memcpy(&live_code_analysis_system.config, config, sizeof(live_analysis_config_t));
}

void LiveCodeAnalysis_GetConfig(live_analysis_config_t* config) {
    if (!config) return;
    memcpy(config, &live_code_analysis_system.config, sizeof(live_analysis_config_t));
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* LiveCodeAnalysis_GetModeName(live_analysis_mode_t mode) {
    switch (mode) {
        case LIVE_MODE_OFF: return "Off";
        case LIVE_MODE_BACKGROUND: return "Background";
        case LIVE_MODE_REALTIME: return "Real-time";
        case LIVE_MODE_INCREMENTAL: return "Incremental";
        default: return "Unknown";
    }
}

const char* LiveCodeAnalysis_GetProtocolName(ide_protocol_t protocol) {
    switch (protocol) {
        case IDE_PROTOCOL_NONE: return "None";
        case IDE_PROTOCOL_LSP: return "Language Server Protocol";
        case IDE_PROTOCOL_VS_CODE: return "VS Code";
        case IDE_PROTOCOL_CLION: return "CLion";
        case IDE_PROTOCOL_VIM: return "Vim";
        case IDE_PROTOCOL_EMACS: return "Emacs";
        default: return "Unknown";
    }
}

const char* LiveCodeAnalysis_GetSeverityName(review_severity_t severity) {
    switch (severity) {
        case REVIEW_SEVERITY_INFO: return "Info";
        case REVIEW_SEVERITY_WARNING: return "Warning";
        case REVIEW_SEVERITY_ERROR: return "Error";
        case REVIEW_SEVERITY_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}
