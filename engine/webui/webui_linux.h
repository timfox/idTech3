#ifndef SURF_WEBUI_LINUX_H
#define SURF_WEBUI_LINUX_H

#include "webui.h"
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WebKitGTK integration for Linux */
bool WebUI_InitLinux(const webui_config_t *config);
void WebUI_PumpEventsLinux(void);
void WebUI_EvaluateJavaScriptLinux(const char *script);
void WebUI_PostGameEventLinux(const char *json);
void WebUI_ShutdownLinux(void);

#ifdef __cplusplus
}
#endif

#endif
