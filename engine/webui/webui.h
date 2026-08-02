#ifndef SURF_WEBUI_H
#define SURF_WEBUI_H

#include <stdbool.h>
#include <stddef.h>

typedef struct webui_config_s {
    const char *title;
    const char *initial_url;
    int width;
    int height;
    bool debug_tools;
    bool transparent;
} webui_config_t;

bool WebUI_Init(const webui_config_t *config);
void WebUI_PumpEvents(void);
void WebUI_EvaluateJavaScript(const char *script);
void WebUI_PostGameEvent(const char *json);
void WebUI_Shutdown(void);

#endif