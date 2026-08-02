#ifndef SURF_WEBUI_MACOS_H
#define SURF_WEBUI_MACOS_H

#include "webui.h"
#include <objc/objc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WKWebView integration for macOS */
bool WebUI_InitMacOS(const webui_config_t *config);
void WebUI_PumpEventsMacOS(void);
void WebUI_EvaluateJavaScriptMacOS(const char *script);
void WebUI_PostGameEventMacOS(const char *json);
void WebUI_ShutdownMacOS(void);

#ifdef __cplusplus
}
#endif

#endif
