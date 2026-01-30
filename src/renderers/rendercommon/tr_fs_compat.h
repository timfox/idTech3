#pragma once

#include "tr_public.h"

// This is a temporary shim: legacy FS_ReadFile takes char* but does not mutate the string.
// Centralize the cast so callsites stay const-correct until the API is upgraded.
static inline int FS_ReadFileConst(const char *path, void **buffer) {
    return ri.FS_ReadFile((char *)path, buffer);
}
