/*
===========================================================================
UI2 - Deterministic UI/Layout System
C API for integration with id Tech 3 engine
===========================================================================
*/

#ifndef __UI2_PUBLIC_H__
#define __UI2_PUBLIC_H__

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct ui2Context_s ui2Context_t;

// Forward declaration
struct fontInfo_s;
typedef struct fontInfo_s fontInfo_t;

// Renderer callbacks (provided by engine)
typedef struct {
	// Set current color (rgba 0.0-1.0, NULL = white)
	void (*SetColor)(const float *rgba);
	
	// Draw stretched picture (for solid rects and text)
	// x, y, w, h in screen pixels
	// s1, t1, s2, t2 are texture coordinates (0.0-1.0)
	// hShader is shader handle (0 = white solid)
	void (*DrawStretchPic)(float x, float y, float w, float h,
	                      float s1, float t1, float s2, float t2,
	                      qhandle_t hShader);
	
	// Enable/disable scissor clipping (optional, can be NULL)
	// x, y, w, h in screen pixels, NULL = disable
	void (*Scissor)(int x, int y, int w, int h);
	
	// Text rendering (optional, can be NULL for placeholder)
	// x, y in screen pixels, scale is font scale, color is rgba [0-1]
	// text is the string to render, font is the font info
	void (*TextPaint)(float x, float y, float scale, const float *color,
	                 const char *text, fontInfo_t *font);
	
	// Get default font (optional, can be NULL)
	fontInfo_t *(*GetDefaultFont)(void);
} ui2Renderer_t;

// Context creation/destruction
ui2Context_t *UI2_CreateContext(const ui2Renderer_t *renderer);
void UI2_DestroyContext(ui2Context_t *ctx);

// Frame lifecycle
void UI2_BeginFrame(ui2Context_t *ctx, int screenWidth, int screenHeight);
void UI2_EndFrame(ui2Context_t *ctx);

// Style sheet loading (CSS-like syntax)
qboolean UI2_LoadStylesheet(ui2Context_t *ctx, const char *cssText);

// Node tree building
void UI2_BeginNode(ui2Context_t *ctx, const char *tag, const char *className);
void UI2_EndNode(ui2Context_t *ctx);

// Text content
void UI2_Text(ui2Context_t *ctx, const char *tag, const char *text);

// Enable/disable UI2 system
void UI2_Init(void);
void UI2_Shutdown(void);
qboolean UI2_IsEnabled(void);

#ifdef __cplusplus
}
#endif

#endif // __UI2_PUBLIC_H__
