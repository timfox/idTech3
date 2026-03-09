#pragma once

#include "tr_public.h"

// This is a temporary shim: legacy FS_ReadFile takes char* but does not mutate the string.
// Avoid const-dropping casts at callsites: feed it a transient mutable copy.
static inline int FS_ReadFileConst(const char *path, void **buffer) {
    return ri.FS_ReadFile(va("%s", path), buffer);
}
