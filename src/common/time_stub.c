#include <time.h>
#include <sys/time.h>
#include "q_shared.h"
#include "qcommon.h"

// Lightweight, cross-platform Milliseconds timer for tests and stubs
int Sys_Milliseconds(void) {
#if defined(_WIN32)
    // Simple fallback on Windows (not expected on Linux CI)
    return (int)(time(NULL) * 1000);
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (int)((long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
    }
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int)((long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL);
#endif
}

