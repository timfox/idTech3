/*
=============================================================================
Live Code Analysis System

Real-time feedback during development with IDE integration and incremental analysis.
=============================================================================
*/

#ifndef __LIVE_CODE_ANALYSIS_H__
#define __LIVE_CODE_ANALYSIS_H__

#include "q_shared.h"
#include "code_review.h"
#include "thread_platform.h"

// Live analysis modes
typedef enum {
    LIVE_MODE_OFF = 0,          // Disabled
    LIVE_MODE_BACKGROUND,       // Background analysis only
    LIVE_MODE_REALTIME,         // Real-time analysis as you type
    LIVE_MODE_INCREMENTAL       // Incremental analysis on file changes
} live_analysis_mode_t;

// IDE integration protocols
typedef enum {
    IDE_PROTOCOL_NONE = 0,      // No IDE integration
    IDE_PROTOCOL_LSP,           // Language Server Protocol
    IDE_PROTOCOL_VS_CODE,       // VS Code specific
    IDE_PROTOCOL_CLION,         // CLion specific
    IDE_PROTOCOL_VIM,           // Vim/Neovim integration
    IDE_PROTOCOL_EMACS          // Emacs integration
} ide_protocol_t;

// Live analysis event types
typedef enum {
    LIVE_EVENT_FILE_OPEN = 0,   // File opened in editor
    LIVE_EVENT_FILE_CLOSE,      // File closed in editor
    LIVE_EVENT_FILE_CHANGE,     // File content changed
    LIVE_EVENT_FILE_SAVE,       // File saved
    LIVE_EVENT_CURSOR_MOVE,     // Cursor position changed
    LIVE_EVENT_SELECTION_CHANGE,// Text selection changed
    LIVE_EVENT_SYNTAX_ERROR,    // Syntax error detected
    LIVE_EVENT_COMPLETION_REQUEST // Code completion requested
} live_event_type_t;

// Live analysis finding with additional metadata
typedef struct {
    code_review_finding_t base; // Base finding from static analysis

    // Live analysis specific data
    uint64_t timestamp;         // When this finding was detected
    uint32_t version;           // File version when detected
    qboolean acknowledged;      // Has developer acknowledged this finding?
    qboolean suppressed;        // Has this finding been suppressed?
    char suppression_reason[256]; // Why it was suppressed

    // IDE integration data
    char ide_marker[32];        // IDE-specific marker (e.g., "error", "warning")
    int ide_severity;           // IDE-specific severity level
    char quick_fix[512];        // Quick fix suggestion for IDE
} live_finding_t;

// Live analysis session
typedef struct {
    char filename[MAX_OSPATH];  // Current file being analyzed
    char* content;              // Current file content
    int content_length;         // Content length
    uint32_t version;           // File version counter
    uint64_t last_modified;     // Last modification timestamp
    qboolean is_open;           // Is file currently open in editor?

    // Analysis state
    live_finding_t* findings;   // Current findings for this file
    uint32_t num_findings;
    uint32_t max_findings;

    // Incremental analysis
    char* previous_content;     // Content from last analysis
    int* changed_lines;         // Lines that have changed
    uint32_t num_changed_lines;
} live_session_t;

// IDE integration interface
typedef struct {
    ide_protocol_t protocol;    // Protocol type

    // Protocol-specific data
    void* protocol_data;        // Protocol-specific context

    // Callback functions
    void (*on_finding_added)(const live_finding_t* finding);
    void (*on_finding_removed)(const char* file, int line, int column);
    void (*on_finding_updated)(const live_finding_t* finding);
    void (*on_diagnostics_ready)(const char* file, const live_finding_t* findings, uint32_t count);
    void (*on_completion_ready)(const char* file, int line, int column, const char** completions, uint32_t count);
} ide_integration_t;

// Live analysis configuration
typedef struct {
    live_analysis_mode_t mode;  // Analysis mode
    ide_integration_t ide;      // IDE integration settings

    // Performance settings
    int analysis_interval_ms;   // How often to run analysis (for background mode)
    int max_file_size_kb;       // Maximum file size to analyze
    int max_findings_per_file;  // Maximum findings per file

    // Real-time settings
    qboolean analyze_on_type;   // Analyze as you type
    qboolean analyze_on_save;   // Analyze on save
    int typing_delay_ms;        // Delay after typing stops before analysis

    // Incremental analysis
    qboolean use_incremental;   // Use incremental analysis
    int incremental_threshold;  // Lines changed before full reanalysis

    // Filtering
    qboolean show_acked_findings; // Show acknowledged findings
    qboolean auto_acknowledge;   // Auto-acknowledge findings on fix
} live_analysis_config_t;

// Live code analysis system
typedef struct {
    qboolean initialized;
    live_analysis_config_t config;

    // Analysis thread
    thread_handle_t analysis_thread;
    condition_t work_available;
    mutex_t work_mutex;
    spinlock_t session_lock;
    qboolean should_exit;

    // Work queue
    void** work_queue;
    uint32_t work_queue_size;
    atomic_uint_t work_queue_head;
    atomic_uint_t work_queue_tail;
    atomic_uint_t work_available_count;

    // Sessions
    live_session_t* sessions;
    uint32_t max_sessions;
    uint32_t num_sessions;

    // Global statistics
    atomic_uint64_t total_analyses;
    atomic_uint64_t total_findings;
    atomic_uint64_t analysis_time_ms;

    // IDE integration
    ide_integration_t* ide_integration;
} live_code_analysis_system_t;

extern live_code_analysis_system_t live_code_analysis_system;

// Live Code Analysis API
qboolean LiveCodeAnalysis_Init(void);
void LiveCodeAnalysis_Shutdown(void);

qboolean LiveCodeAnalysis_SetMode(live_analysis_mode_t mode);
live_analysis_mode_t LiveCodeAnalysis_GetMode(void);

qboolean LiveCodeAnalysis_SetIDEProtocol(ide_protocol_t protocol);
ide_protocol_t LiveCodeAnalysis_GetIDEProtocol(void);

// Session management
live_session_t* LiveCodeAnalysis_OpenFile(const char* filename);
qboolean LiveCodeAnalysis_CloseFile(const char* filename);
live_session_t* LiveCodeAnalysis_GetSession(const char* filename);

// Content updates
qboolean LiveCodeAnalysis_UpdateContent(const char* filename, const char* content, int length);
qboolean LiveCodeAnalysis_UpdateRegion(const char* filename, int start_line, int end_line, const char* new_content);

// Event handling
void LiveCodeAnalysis_OnFileEvent(live_event_type_t event, const char* filename,
                                 int line, int column, const char* data);

// Finding management
uint32_t LiveCodeAnalysis_GetFindings(const char* filename, live_finding_t** findings);
qboolean LiveCodeAnalysis_AcknowledgeFinding(const char* filename, int line, int column);
qboolean LiveCodeAnalysis_SuppressFinding(const char* filename, int line, int column, const char* reason);

// Real-time analysis
qboolean LiveCodeAnalysis_AnalyzeNow(const char* filename);
qboolean LiveCodeAnalysis_AnalyzeIncremental(const char* filename, int start_line, int end_line);

// IDE integration
void LiveCodeAnalysis_SendToIDE(const char* filename, const live_finding_t* findings, uint32_t count);
void LiveCodeAnalysis_RequestCompletion(const char* filename, int line, int column);

// Performance monitoring
void LiveCodeAnalysis_GetStats(uint64_t* total_analyses, uint64_t* total_findings, uint64_t* analysis_time);

// Configuration
void LiveCodeAnalysis_SetConfig(const live_analysis_config_t* config);
void LiveCodeAnalysis_GetConfig(live_analysis_config_t* config);

// Utility functions
const char* LiveCodeAnalysis_GetModeName(live_analysis_mode_t mode);
const char* LiveCodeAnalysis_GetProtocolName(ide_protocol_t protocol);
const char* LiveCodeAnalysis_GetSeverityName(review_severity_t severity);

#endif // __LIVE_CODE_ANALYSIS_H__
