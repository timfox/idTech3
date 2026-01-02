#include "crash_verbose.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Path to crash report; adjust if you relocate the repo
// Crash report path; relocate if needed
#define CRASH_REPORT_PATH "/home/tim/Desktop/idtech3/logs/crash_report.txt"
// Additional verbose crash log location
#define CRASH_VERBOSE_LOG "/home/tim/Desktop/idtech3/logs/crash_verbose.log"
// Relocated crash logs directory
#define CRASH_LOG_DIR "/home/tim/Desktop/idtech3/logs"

void append_verbose_crash_context(void) {
    void* buffer[256];
    int nptrs = backtrace(buffer, sizeof(buffer) / sizeof(void*));
    if (nptrs <= 0) return;

    char** strings = backtrace_symbols(buffer, nptrs);
    if (!strings) return;

    // Persist to primary crash report
    FILE* f = fopen(CRASH_REPORT_PATH, "a");
    if (!f) {
        free(strings);
        return;
    }
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timebuf[64];
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
    FILE* f3 = fopen("/home/tim/Desktop/idtech3/logs/collected_crash_report.txt", "a");
    if (f3) {
        time_t now3 = time(NULL);
        fprintf(f3, "Crash at %ld (verbose log updated)\n", now3);
        fclose(f3);
    }
}
