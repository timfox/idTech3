#ifndef SURF_WEBUI_ANDROID_H
#define SURF_WEBUI_ANDROID_H

#include "webui.h"

bool WebUI_InitAndroid(const webui_config_t *config);
void WebUI_PumpEventsAndroid(void);
void WebUI_EvaluateJavaScriptAndroid(const char *script);
void WebUI_PostGameEventAndroid(const char *json);
void WebUI_ShutdownAndroid(void);

#endif