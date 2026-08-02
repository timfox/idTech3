#ifndef SURF_WEBUI_WIN32_H
#define SURF_WEBUI_WIN32_H

#include "webui.h"
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WebView2 integration for Windows */
bool WebUI_InitWin32(const webui_config_t *config);
void WebUI_PumpEventsWin32(void);
void WebUI_EvaluateJavaScriptWin32(const char *script);
void WebUI_PostGameEventWin32(const char *json);
void WebUI_ShutdownWin32(void);

#ifdef __cplusplus
}
#endif

#endif
