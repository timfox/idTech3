#include "crash_verbose.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Path to crash report; adjust if you relocate the repo
#define CRASH_REPORT_PATH "/home/tim/Desktop/idtech3/crash_report.txt"

void append_verbose_crash_context(void) {
    void* buffer[256];
    int nptrs = backtrace(buffer, sizeof(buffer) / sizeof(void*));
    if (nptrs <= 0) return;

    char** strings = backtrace_symbols(buffer, nptrs);
    if (!strings) return;

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
}
