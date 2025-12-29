#include <stdlib.h>
#include <stdio.h>

#if defined(__GNUC__)
#define AUTO_CONSTRUCTOR __attribute__((constructor))
#else
#define AUTO_CONSTRUCTOR
#endif

// This file hooks into process startup to optionally auto-load a map
// specified by the AUTO_MAP_NAME environment variable.
extern void Cbuf_AddText(const char* text);

static void auto_map_startup_run(void) AUTO_CONSTRUCTOR;
static void auto_map_startup_run(void) {
    const char* map_name = getenv("AUTO_MAP_NAME");
    if (map_name && map_name[0] != '\0') {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "devmap %s\n", map_name);
        // Enqueue the map load into the in-game console buffer
        Cbuf_AddText(cmd);
        fprintf(stderr, "AUTO_MAP_STARTUP: auto-loading map '%s' (AUTO_MAP_NAME)\n", map_name);
    }
}
