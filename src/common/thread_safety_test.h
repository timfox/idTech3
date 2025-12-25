/*
=============================================================================
Thread Safety Test Framework

Race condition detection and validation using ThreadSanitizer and concurrency testing.
=============================================================================
*/

#ifndef __THREAD_SAFETY_TEST_H__
#define __THREAD_SAFETY_TEST_H__

#include "q_shared.h"
#include "thread_platform.h"

// Thread safety test result types
typedef enum {
    THREAD_RESULT_PASS = 0,         // Test passed without issues
    THREAD_RESULT_RACE_CONDITION,   // Data race detected
    THREAD_RESULT_DEADLOCK,         // Deadlock detected
    THREAD_RESULT_DATA_RACE,        // Data race (alias for race condition)
    THREAD_RESULT_ATOMIC_VIOLATION, // Atomic operation violation
    THREAD_RESULT_LOCK_ORDER_VIOLATION, // Lock order violation
    THREAD_RESULT_TIMEOUT,          // Test timed out
    THREAD_RESULT_CRASH,            // Test crashed
    THREAD_RESULT_INCOMPLETE,       // Test did not complete
    THREAD_RESULT_COUNT
} thread_safety_result_t;

// ThreadSanitizer error types
typedef enum {
    TSAN_ERROR_NONE = 0,
    TSAN_ERROR_DATA_RACE,           // Data race between threads
    TSAN_ERROR_USE_OF_UNINITIALIZED,// Use of uninitialized value
    TSAN_ERROR_RACE_ON_MUTEX,       // Race on mutex operations
    TSAN_ERROR_DESTROY_OF_LOCKED_MUTEX, // Destroying locked mutex
    TSAN_ERROR_DOUBLE_LOCK,         // Double lock of mutex
    TSAN_ERROR_UNLOCK_OF_UNLOCKED_MUTEX, // Unlock of unlocked mutex
    TSAN_ERROR_INVALID_MUTEX_ORDER, // Mutex lock order violation
    TSAN_ERROR_RACE_ON_ATOMIC,      // Race on atomic operations
    TSAN_ERROR_UNKNOWN
} tsan_error_type_t;

// Thread safety test configuration
typedef struct {
    char test_name[64];
    char description[256];
    qboolean enable_tsan;           // Enable ThreadSanitizer
    qboolean enable_deadlock_detection; // Enable deadlock detection
    qboolean enable_race_detection; // Enable race condition detection
    qboolean strict_mode;           // Treat warnings as errors
    int timeout_seconds;            // Test timeout
    int num_threads;                // Number of threads to use
    int iterations;                 // Number of test iterations
    qboolean randomize_timing;      // Randomize thread timing
    char setup_script[256];         // Thread setup script
} thread_safety_test_config_t;

// ThreadSanitizer error report
typedef struct {
    tsan_error_type_t error_type;   // TSan error classification
    char function1[128];            // First conflicting function
    char file1[256];                // First conflicting file
    int line1;                      // First conflicting line
    char function2[128];            // Second conflicting function
    char file2[256];                // Second conflicting file
    int line2;                      // Second conflicting line
    char access_type1[16];          // First access type (read/write)
    char access_type2[16];          // Second access type (read/write)
    char description[1024];         // Error description
    char stack_trace[4096];         // Stack trace
    qboolean is_fatal;              // Whether this is a fatal error
    uint64_t timestamp;             // When error was detected
    int thread_id1;                 // First thread ID
    int thread_id2;                 // Second thread ID
} tsan_error_t;

// Thread safety test result
typedef struct {
    char test_name[64];
    thread_safety_result_t result;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration_ms;

    // Error details
    tsan_error_t* errors;
    uint32_t error_count;
    uint32_t max_errors;

    // Thread statistics
    int num_threads_created;
    int max_concurrent_threads;
    uint64_t total_context_switches;
    uint64_t total_lock_contention;

    // Test metadata
    char platform[32];
    char compiler[32];
    char tsan_version[32];
    qboolean tsan_enabled;
    int test_iterations_completed;
} thread_safety_test_result_t;

// Thread safety test suite
typedef struct {
    char suite_name[64];
    char description[256];
    thread_safety_test_config_t* tests;
    uint32_t num_tests;
    uint32_t max_tests;
    qboolean enable_tsan;
    qboolean enable_deadlock_detection;
    qboolean enable_race_detection;
    qboolean strict_mode;
    int suite_timeout_seconds;
    int default_num_threads;
    int default_iterations;
} thread_safety_test_suite_t;

// Thread safety testing system
typedef struct {
    qboolean initialized;
    thread_safety_test_suite_t* current_suite;
    thread_safety_test_result_t* results;
    uint32_t max_results;
    uint32_t num_results;

    // TSan configuration
    qboolean tsan_available;
    char tsan_version[32];

    // Threading capabilities
    int max_supported_threads;
    qboolean supports_thread_sanitizer;
    qboolean supports_deadlock_detection;

    // Statistics
    uint32_t total_tests_run;
    uint32_t total_races_detected;
    uint32_t total_deadlocks_detected;
    uint32_t total_atomic_violations;
    uint32_t total_crashes;
} thread_safety_test_system_t;

extern thread_safety_test_system_t thread_safety_test_system;

// Thread Safety Test API
qboolean ThreadSafetyTest_Init(void);
void ThreadSafetyTest_Shutdown(void);

// TSan Detection and Configuration
qboolean ThreadSafetyTest_DetectTSan(void);
qboolean ThreadSafetyTest_EnableTSan(void);
qboolean ThreadSafetyTest_EnableDeadlockDetection(void);
qboolean ThreadSafetyTest_EnableRaceDetection(void);
qboolean ThreadSafetyTest_SetStrictMode(qboolean strict);

// Test Suite Management
thread_safety_test_suite_t* ThreadSafetyTest_CreateSuite(const char* name, const char* description);
qboolean ThreadSafetyTest_AddTestToSuite(thread_safety_test_suite_t* suite,
                                       const thread_safety_test_config_t* config);
qboolean ThreadSafetyTest_RunSuite(thread_safety_test_suite_t* suite);

// Individual Test Execution
qboolean ThreadSafetyTest_RunTest(const thread_safety_test_config_t* config,
                                thread_safety_test_result_t* result);
qboolean ThreadSafetyTest_CancelTest(void);
qboolean ThreadSafetyTest_IsTestRunning(void);

// Test Result Management
uint32_t ThreadSafetyTest_GetResults(thread_safety_test_result_t** results);
qboolean ThreadSafetyTest_SaveResults(const char* filename);
qboolean ThreadSafetyTest_LoadResults(const char* filename);

// Error Analysis
tsan_error_type_t ThreadSafetyTest_ClassifyTSanError(const char* error_msg);
qboolean ThreadSafetyTest_ParseTSanOutput(const char* output,
                                        tsan_error_t* errors,
                                        uint32_t max_errors,
                                        uint32_t* num_errors);

// Built-in Test Functions
qboolean ThreadSafetyTest_DataRaceBasic(void);
qboolean ThreadSafetyTest_DataRaceAtomic(void);
qboolean ThreadSafetyTest_DeadlockMutex(void);
qboolean ThreadSafetyTest_DeadlockRwLock(void);
qboolean ThreadSafetyTest_LockOrderViolation(void);
qboolean ThreadSafetyTest_UseAfterFreeConcurrent(void);
qboolean ThreadSafetyTest_DoubleLock(void);
qboolean ThreadSafetyTest_UnlockUnlockedMutex(void);
qboolean ThreadSafetyTest_ConditionVariableRace(void);
qboolean ThreadSafetyTest_SemaphoreRace(void);

// Comprehensive Thread Safety Tests
qboolean ThreadSafetyTest_MemoryAllocator(void);
qboolean ThreadSafetyTest_SharedDataStructures(void);
qboolean ThreadSafetyTest_ThreadPool(void);
qboolean ThreadSafetyTest_MessageQueues(void);
qboolean ThreadSafetyTest_ResourceManagement(void);
qboolean ThreadSafetyTest_SynchronizationPrimitives(void);
qboolean ThreadSafetyTest_LockFreeDataStructures(void);
qboolean ThreadSafetyTest_AtomicOperations(void);

// Stress Testing
qboolean ThreadSafetyTest_HighContention(void);
qboolean ThreadSafetyTest_LongRunningThreads(void);
qboolean ThreadSafetyTest_FrequentContextSwitches(void);
qboolean ThreadSafetyTest_MaxThreads(void);

// Automated Test Generation
qboolean ThreadSafetyTest_GenerateRaceConditionTests(thread_safety_test_suite_t* suite);
qboolean ThreadSafetyTest_GenerateDeadlockTests(thread_safety_test_suite_t* suite);
qboolean ThreadSafetyTest_GenerateStressTests(thread_safety_test_suite_t* suite);

// CI/CD Integration
qboolean ThreadSafetyTest_ExportForCI(const char* output_dir);
qboolean ThreadSafetyTest_GenerateReport(const char* output_file, const char* format);

// Utility Functions
const char* ThreadSafetyTest_GetResultString(thread_safety_result_t result);
const char* ThreadSafetyTest_GetTSanErrorString(tsan_error_type_t error);
qboolean ThreadSafetyTest_ValidateTestConfig(const thread_safety_test_config_t* config);

// Thread Creation and Management
thread_handle_t ThreadSafetyTest_CreateTestThread(thread_func_t func, void* arg);
void ThreadSafetyTest_WaitForTestThreads(thread_handle_t* threads, int num_threads);
void ThreadSafetyTest_CleanupTestThreads(thread_handle_t* threads, int num_threads);

// Synchronization Primitives for Testing
void* ThreadSafetyTest_SharedCounter(void* arg);
void* ThreadSafetyTest_SharedList(void* arg);
void* ThreadSafetyTest_SharedHashMap(void* arg);
void* ThreadSafetyTest_ProducerConsumer(void* arg);
void* ThreadSafetyTest_ReadersWriters(void* arg);

#endif // __THREAD_SAFETY_TEST_H__
