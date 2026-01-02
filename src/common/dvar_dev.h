#ifndef DVARD_DEV_H
#define DVARD_DEV_H

#include <stdlib.h>

// Lightweight developer gate: true if DEVELOPER env var is set to non-zero
static inline int Dev_IsEnabled(void) {
    const char *v = getenv("DEVELOPER");
    if (!v) return 0;
    return atoi(v) != 0;
}

#endif // DVARD_DEV_H
