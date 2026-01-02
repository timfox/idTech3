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

void append_verbose_crash_context(void) {
    void* buffer[256];
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
    struct tm* tm_info = localtime(&now);
    char timebuf[64];
    char crash_filename[256];
    strftime(timebuf, sizeof(timebuf), "%Y%m%d_%H%M%S", tm_info);
    snprintf(crash_filename, sizeof(crash_filename), "%s%s.txt", CRASH_REPORT_BASE, timebuf);

    // Persist to primary crash report
    FILE* f = fopen(crash_filename, "a");
    if (!f) {
        free(strings);
        return;
    }
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(f, "\n=== Backtrace at %s ===\n", timebuf);
    for (int i = 0; i < nptrs; ++i) {
        fprintf(f, "%p %s\n", buffer[i], strings[i]);
    }
    fprintf(f, "=== End Backtrace ===\n");
    fclose(f);
    free(strings);
    // Additionally, write to a verbose crash log if available
    FILE* f2 = fopen(CRASH_VERBOSE_LOG, "a");
    if (f2) {
        time_t now2 = time(NULL);
        struct tm* tm2 = localtime(&now2);
        char tbuf[64];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm2);
        fprintf(f2, "\n=== Verbose Crash at %s ===\n", tbuf);
        for (int i = 0; i < nptrs; ++i) {
            fprintf(f2, "%p %s\n", buffer[i], strings[i]);
        }
        fprintf(f2, "=== End Verbose Crash ===\n");
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
        fprintf(f3, "Build: %s\n", "dev-unknown"); // Could be passed as parameter
        fclose(f3);
    }
}
