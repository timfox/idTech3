#include "crash_verbose.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Base path for crash reports; uses relative path from working directory
#define CRASH_REPORT_BASE "logs/crash_report_"
// Additional verbose crash log location
#define CRASH_VERBOSE_LOG "logs/crash_verbose.log"
// Crash logs directory
#define CRASH_LOG_DIR "logs"

// Maximum backtrace depth - can be increased if needed for very deep call stacks
#define MAX_BACKTRACE_DEPTH 256

void append_verbose_crash_context(void) {
    void* buffer[MAX_BACKTRACE_DEPTH];
    int nptrs = backtrace(buffer, sizeof(buffer) / sizeof(void*));
    if (nptrs <= 0) return;

    char** strings = backtrace_symbols(buffer, nptrs);
    if (!strings) return;

    // Ensure logs directory exists
    struct stat st = {0};
    if (stat(CRASH_LOG_DIR, &st) == -1) {
        mkdir(CRASH_LOG_DIR, 0755);
    }

    // Generate timestamped crash report filename
    time_t now = time(NULL);
    struct tm tm_info_buf;
    struct tm* tm_info = localtime_r(&now, &tm_info_buf);
    if (!tm_info) {
        free(strings);
        return; // Failed to get local time
    }
    char timebuf[64];
    char crash_filename[256];
    if (strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", tm_info) == 0) {
        free(strings);
        return; // Failed to format time
    }
    if (snprintf(crash_filename, sizeof(crash_filename), "%s%s.txt", CRASH_REPORT_BASE, timebuf) >= (int)sizeof(crash_filename)) {
        free(strings);
        return; // Buffer overflow prevented
    }

    // Persist to primary crash report
    FILE* f = fopen(crash_filename, "a");
    if (!f) {
        free(strings);
        return;
    }
    if (strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        fclose(f);
        free(strings);
        return; // Failed to format time
    }
    if (fprintf(f, "\n=== Backtrace at %s ===\n", timebuf) < 0) {
        fclose(f);
        free(strings);
        return; // Failed to write to file
    }
    for (int i = 0; i < nptrs; ++i) {
        if (fprintf(f, "%p %s\n", buffer[i], strings[i]) < 0) {
            fclose(f);
            free(strings);
            return; // Failed to write to file
        }
    }
    if (fprintf(f, "=== End Backtrace ===\n") < 0) {
        fclose(f);
        free(strings);
        return; // Failed to write to file
    }
    fclose(f);
    free(strings);
    // Additionally, write to a verbose crash log if available
    FILE* f2 = fopen(CRASH_VERBOSE_LOG, "a");
    if (f2) {
        time_t now2 = time(NULL);
        struct tm tm2_buf;
        struct tm* tm2 = localtime_r(&now2, &tm2_buf);
        if (tm2) {
            char tbuf[64];
            if (strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm2) > 0) {
                if (fprintf(f2, "\n=== Verbose Crash at %s ===\n", tbuf) >= 0) {
                    for (int i = 0; i < nptrs; ++i) {
                        if (fprintf(f2, "%p %s\n", buffer[i], strings[i]) < 0) {
                            break; // Stop writing on error
                        }
                    }
                    fprintf(f2, "=== End Verbose Crash ===\n");
                }
            }
        }
        fclose(f2);
    }
    // Also create a lightweight relocation note file for automation
    char collection_filename[256];
    strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", tm_info);
    snprintf(collection_filename, sizeof(collection_filename), "logs/crash_collection_%s.txt", timebuf);

    FILE* f3 = fopen(collection_filename, "w");
    if (f3) {
        fprintf(f3, "Crash Report Generated: %s\n", crash_filename);
        fprintf(f3, "Timestamp: %ld\n", (long)now);
        fprintf(f3, "Build: %s\n", BUILD_ID);
        fprintf(f3, "Build Date: %s\n", BUILD_DATE);
        fclose(f3);
    }
}
