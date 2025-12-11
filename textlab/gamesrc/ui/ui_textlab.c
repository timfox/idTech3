#include "q_shared.h"
#include "ui_public.h"
#include "keycodes.h"
#include "tr_types.h"

// Shared syscall entry point
intptr_t (QDECL *ui_syscall)(intptr_t arg, ...) = NULL;

static glconfig_t ui_glconfig;
static fontInfo_t ui_font;
static qhandle_t ui_white_shader = 0;
static float ui_base_scale = 1.0f; // inverse glyphScale so texels map 1:1

static int PASSFLOAT(float x) {
	int i;
	memcpy(&i, &x, sizeof(i));
	return i;
}

// --- syscall wrappers --------------------------------------------------------
static inline void UI_Printf(const char *fmt, ...) {
	if (!ui_syscall) {
		return;
	}
	char buffer[1024];
	va_list argptr;
	va_start(argptr, fmt);
	Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
	va_end(argptr);
	ui_syscall(UI_PRINT, buffer);
}

static inline void trap_GetGlconfig(glconfig_t *glconfig) {
	if (ui_syscall) {
		ui_syscall(UI_GETGLCONFIG, glconfig);
	}
}

static inline qhandle_t trap_R_RegisterShaderNoMip(const char *name) {
	return ui_syscall ? ui_syscall(UI_R_REGISTERSHADERNOMIP, name) : 0;
}

static inline void trap_R_SetColor(const float *rgba) {
	if (ui_syscall) {
		ui_syscall(UI_R_SETCOLOR, rgba);
	}
}

static inline void trap_R_DrawStretchPic(float x, float y, float w, float h,
                                         float s1, float t1, float s2, float t2,
                                         qhandle_t shader) {
	if (ui_syscall) {
		ui_syscall(UI_R_DRAWSTRETCHPIC,
		           PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h),
		           PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2),
		           shader);
	}
}

static inline void trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	if (ui_syscall) {
		ui_syscall(UI_R_REGISTERFONT, fontName, pointSize, font);
	}
}

static inline int trap_FS_FOpenFile(const char *qpath, fileHandle_t *f, fsMode_t mode) {
	return ui_syscall ? ui_syscall(UI_FS_FOPENFILE, qpath, f, mode) : 0;
}

static inline int trap_FS_Read(void *buffer, int len, fileHandle_t f) {
	return ui_syscall ? ui_syscall(UI_FS_READ, buffer, len, f) : 0;
}

static inline int trap_FS_FCloseFile(fileHandle_t f) {
	return ui_syscall ? ui_syscall(UI_FS_FCLOSEFILE, f) : 0;
}

static inline void trap_Cmd_ExecuteText(int when, const char *text) {
	if (ui_syscall) {
		ui_syscall(UI_CMD_EXECUTETEXT, when, text);
	}
}

static inline int trap_Key_GetCatcher(void) {
	return ui_syscall ? ui_syscall(UI_KEY_GETCATCHER) : 0;
}

static inline void trap_Key_SetCatcher(int catcher) {
	if (ui_syscall) {
		ui_syscall(UI_KEY_SETCATCHER, catcher);
	}
}

// --- helpers -----------------------------------------------------------------
static float UI_To640X(float px) {
	return (ui_glconfig.vidWidth > 0) ? (px * 640.0f / (float)ui_glconfig.vidWidth) : px;
}

static float UI_To640Y(float py) {
	return (ui_glconfig.vidHeight > 0) ? (py * 480.0f / (float)ui_glconfig.vidHeight) : py;
}

static void UI_DrawSolid(float x, float y, float w, float h, const float color[4]) {
	if (!ui_white_shader) {
		return;
	}
	trap_R_SetColor(color);
	trap_R_DrawStretchPic(UI_To640X(x), UI_To640Y(y), UI_To640X(w), UI_To640Y(h),
	                      0.0f, 0.0f, 1.0f, 1.0f, ui_white_shader);
	trap_R_SetColor(NULL);
}

static void UI_DrawStringPixels(float x, float y, const char *text, const float color[4]) {
	if (!text || !ui_font.name[0]) {
		return;
	}

	float useScale = ui_base_scale * ui_font.glyphScale;
	const unsigned char *s = (const unsigned char *)text;
	float cursorX = x;
	float cursorY = y;

	trap_R_SetColor(color);

	while (s && *s) {
		if (Q_IsColorString((const char *)s)) {
			s += 2;
			continue;
		}

		unsigned char c = *s;
		glyphInfo_t *glyph = &ui_font.glyphs[c];
		if (glyph->glyph != 0) {
			float yAdj = useScale * glyph->top;
			float drawX = cursorX;
			float drawY = cursorY - yAdj;
			float w = glyph->imageWidth * useScale;
			float h = glyph->imageHeight * useScale;

			trap_R_DrawStretchPic(UI_To640X(drawX), UI_To640Y(drawY),
			                      UI_To640X(w), UI_To640Y(h),
			                      glyph->s, glyph->t, glyph->s2, glyph->t2,
			                      glyph->glyph);
			cursorX += glyph->xSkip * useScale;
		}

		s++;
	}

	trap_R_SetColor(NULL);
}

static void UI_LoadFont(void) {
	fileHandle_t f = 0;
	int len = trap_FS_FOpenFile("fonts/fonts.cfg", &f, FS_READ);
	if (!f || len <= 0 || len >= 8192) {
		UI_Printf("textlab: fonts.cfg missing or invalid, using builtin fallback\n");
		trap_R_RegisterFont("fonts/Roboto-Regular.ttf", 32, &ui_font);
		return;
	}

	static char buffer[8192];
	int bytes = trap_FS_Read(buffer, len, f);
	trap_FS_FCloseFile(f);
	buffer[bytes] = '\0';

	const char *p = buffer;
	const char *token;
	while (1) {
		token = COM_ParseExt(&p, qtrue);
		if (!token[0]) {
			break;
		}
		if (!Q_stricmp(token, "font")) {
			const char *fontName = COM_ParseExt(&p, qtrue);
			const char *sizeTok = COM_ParseExt(&p, qtrue);
			int pointSize = sizeTok[0] ? atoi(sizeTok) : 32;
			trap_R_RegisterFont(fontName, pointSize, &ui_font);
			break;
		}
	}

	if (!ui_font.name[0]) {
		UI_Printf("textlab: no font entry found, using fallback\n");
		trap_R_RegisterFont("fonts/Roboto-Regular.ttf", 32, &ui_font);
	}
}

static void UI_DrawHarness(void) {
	static const vec4_t bg = {0.05f, 0.05f, 0.05f, 1.0f};
	static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};

	UI_DrawSolid(0.0f, 0.0f, (float)ui_glconfig.vidWidth, (float)ui_glconfig.vidHeight, bg);
	UI_DrawStringPixels(100.0f, 120.0f, "TEST", white);
	UI_DrawStringPixels(100.0f, 164.0f, "The quick brown fox jumps over the lazy dog.", white);
	UI_DrawStringPixels(100.0f, 208.0f, "ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż", white);
}

// --- exports -----------------------------------------------------------------
Q_EXPORT void dllEntry(intptr_t (QDECL *syscallptr)(intptr_t arg, ...)) {
	ui_syscall = syscallptr;
	UI_Printf("textlab UI bound\n");
}

Q_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int arg2,
                         int arg3, int arg4, int arg5, int arg6,
                         int arg7, int arg8, int arg9, int arg10,
                         int arg11) {
	(void)arg2; (void)arg3; (void)arg4; (void)arg5;
	(void)arg6; (void)arg7; (void)arg8; (void)arg9;
	(void)arg10; (void)arg11;

	switch (command) {
	case UI_GETAPIVERSION:
		return UI_API_VERSION;
	case UI_INIT:
		trap_GetGlconfig(&ui_glconfig);
		trap_Key_SetCatcher(trap_Key_GetCatcher() | KEYCATCH_UI);
		UI_LoadFont();
		if (ui_font.glyphScale > 0.0f) {
			ui_base_scale = 1.0f / ui_font.glyphScale;
		}
		ui_white_shader = trap_R_RegisterShaderNoMip("white");
		UI_Printf("textlab: ui init (%dx%d) glyphScale=%.3f baseScale=%.3f\n",
		          ui_glconfig.vidWidth, ui_glconfig.vidHeight,
		          ui_font.glyphScale, ui_base_scale);
		return 0;
	case UI_SHUTDOWN:
		UI_Printf("textlab: ui shutdown\n");
		return 0;
	case UI_REFRESH:
		UI_DrawHarness();
		return 0;
	case UI_KEY_EVENT:
		// arg0 = key, arg1 = down
		if (arg1 && arg0 == K_ESCAPE) {
			trap_Cmd_ExecuteText(EXEC_APPEND, "quit\n");
		}
		return 0;
	case UI_SET_ACTIVE_MENU:
		if (arg0 == UIMENU_NONE) {
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
		} else {
			trap_Key_SetCatcher(trap_Key_GetCatcher() | KEYCATCH_UI);
		}
		return 0;
	case UI_IS_FULLSCREEN:
		return qtrue;
	case UI_CONSOLE_COMMAND:
	default:
		return 0;
	}
}
