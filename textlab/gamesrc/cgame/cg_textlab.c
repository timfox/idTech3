#include "q_shared.h"
#include "cg_public.h"
#include "tr_types.h"

// Shared syscall entry point
intptr_t (QDECL *cg_syscall)(intptr_t arg, ...) = NULL;

// Local state for the harness
static glconfig_t cg_glconfig;
static fontInfo_t cg_font;
static qhandle_t cg_white_shader = 0;
static float cg_base_scale = 1.0f; // inverse glyphScale so atlas texels map 1:1 to pixels

static int PASSFLOAT(float x) {
	int i;
	memcpy(&i, &x, sizeof(i));
	return i;
}

// --- syscall wrappers --------------------------------------------------------
static inline void CG_Printf(const char *fmt, ...) {
	if (!cg_syscall) {
		return;
	}
	char buffer[1024];
	va_list argptr;
	va_start(argptr, fmt);
	Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
	va_end(argptr);
	cg_syscall(CG_PRINT, buffer);
}

static inline void trap_GetGlconfig(glconfig_t *glconfig) {
	if (cg_syscall) {
		cg_syscall(CG_GETGLCONFIG, glconfig);
	}
}

static inline qhandle_t trap_R_RegisterShaderNoMip(const char *name) {
	return cg_syscall ? cg_syscall(CG_R_REGISTERSHADERNOMIP, name) : 0;
}

static inline void trap_R_SetColor(const float *rgba) {
	if (cg_syscall) {
		cg_syscall(CG_R_SETCOLOR, rgba);
	}
}

static inline void trap_R_DrawStretchPic(float x, float y, float w, float h,
                                         float s1, float t1, float s2, float t2,
                                         qhandle_t shader) {
	if (cg_syscall) {
		cg_syscall(CG_R_DRAWSTRETCHPIC,
		           PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h),
		           PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2),
		           shader);
	}
}

static inline void trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	if (cg_syscall) {
		cg_syscall(CG_R_REGISTERFONT, fontName, pointSize, font);
	}
}

static inline int trap_FS_FOpenFile(const char *qpath, fileHandle_t *f, fsMode_t mode) {
	return cg_syscall ? cg_syscall(CG_FS_FOPENFILE, qpath, f, mode) : 0;
}

static inline int trap_FS_Read(void *buffer, int len, fileHandle_t f) {
	return cg_syscall ? cg_syscall(CG_FS_READ, buffer, len, f) : 0;
}

static inline int trap_FS_FCloseFile(fileHandle_t f) {
	return cg_syscall ? cg_syscall(CG_FS_FCLOSEFILE, f) : 0;
}

// --- helpers -----------------------------------------------------------------
static float CG_To640X(float px) {
	return (cg_glconfig.vidWidth > 0) ? (px * 640.0f / (float)cg_glconfig.vidWidth) : px;
}

static float CG_To640Y(float py) {
	return (cg_glconfig.vidHeight > 0) ? (py * 480.0f / (float)cg_glconfig.vidHeight) : py;
}

static void CG_DrawSolid(float x, float y, float w, float h, const float color[4]) {
	if (!cg_white_shader) {
		return;
	}
	trap_R_SetColor(color);
	trap_R_DrawStretchPic(CG_To640X(x), CG_To640Y(y), CG_To640X(w), CG_To640Y(h),
	                      0.0f, 0.0f, 1.0f, 1.0f, cg_white_shader);
	trap_R_SetColor(NULL);
}

static void CG_DrawStringPixels(float x, float y, const char *text, const float color[4]) {
	if (!text || !cg_font.name[0]) {
		return;
	}

	float useScale = cg_base_scale * cg_font.glyphScale; // expect 1.0 when glyphScale is valid
	const unsigned char *s = (const unsigned char *)text;
	float cursorX = x;
	float cursorY = y;

	trap_R_SetColor(color);

	while (s && *s) {
		if (Q_IsColorString((const char *)s)) {
			s += 2; // skip color codes for now
			continue;
		}

		unsigned char c = *s;
		glyphInfo_t *glyph = &cg_font.glyphs[c];
		if (glyph->glyph != 0) {
			float yAdj = useScale * glyph->top;
			float drawX = cursorX;
			float drawY = cursorY - yAdj;
			float w = glyph->imageWidth * useScale;
			float h = glyph->imageHeight * useScale;

			trap_R_DrawStretchPic(CG_To640X(drawX), CG_To640Y(drawY),
			                      CG_To640X(w), CG_To640Y(h),
			                      glyph->s, glyph->t, glyph->s2, glyph->t2,
			                      glyph->glyph);
			cursorX += glyph->xSkip * useScale;
		}

		s++;
	}

	trap_R_SetColor(NULL);
}

static void CG_LoadFont(void) {
	// Simple parser: first 'font' entry wins
	fileHandle_t f = 0;
	int len = trap_FS_FOpenFile("fonts/fonts.cfg", &f, FS_READ);
	if (!f || len <= 0 || len >= 8192) {
		CG_Printf("textlab: fonts.cfg missing or invalid, using builtin fallback\n");
		trap_R_RegisterFont("fonts/Roboto-Regular.ttf", 32, &cg_font);
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
			trap_R_RegisterFont(fontName, pointSize, &cg_font);
			break; // stop after first primary font
		}
	}

	if (!cg_font.name[0]) {
		CG_Printf("textlab: no font entry found, using fallback\n");
		trap_R_RegisterFont("fonts/Roboto-Regular.ttf", 32, &cg_font);
	}
}

static void CG_DrawHarness(void) {
	static const vec4_t bg = {0.05f, 0.05f, 0.05f, 1.0f};
	static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};

	// Background fill
	CG_DrawSolid(0.0f, 0.0f, (float)cg_glconfig.vidWidth, (float)cg_glconfig.vidHeight, bg);

	// Fixed test strings at integer pixel positions
	CG_DrawStringPixels(100.0f, 120.0f, "TEST", white);
	CG_DrawStringPixels(100.0f, 164.0f, "The quick brown fox jumps over the lazy dog.", white);
	CG_DrawStringPixels(100.0f, 208.0f, "ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż", white);
}

// --- exports -----------------------------------------------------------------
Q_EXPORT void dllEntry(intptr_t (QDECL *syscallptr)(intptr_t arg, ...)) {
	cg_syscall = syscallptr;
	CG_Printf("textlab cgame bound\n");
}

Q_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int arg2,
                         int arg3, int arg4, int arg5, int arg6,
                         int arg7, int arg8, int arg9, int arg10,
                         int arg11) {
	(void)arg0; (void)arg1; (void)arg2; (void)arg3;
	(void)arg4; (void)arg5; (void)arg6; (void)arg7;
	(void)arg8; (void)arg9; (void)arg10; (void)arg11;

	switch (command) {
	case CG_INIT:
		trap_GetGlconfig(&cg_glconfig);
		CG_LoadFont();
		if (cg_font.glyphScale > 0.0f) {
			cg_base_scale = 1.0f / cg_font.glyphScale;
		}
		cg_white_shader = trap_R_RegisterShaderNoMip("white");
		CG_Printf("textlab: init (%dx%d), glyphScale=%.3f baseScale=%.3f\n",
		          cg_glconfig.vidWidth, cg_glconfig.vidHeight,
		          cg_font.glyphScale, cg_base_scale);
		return 0;
	case CG_SHUTDOWN:
		CG_Printf("textlab: shutdown\n");
		return 0;
	case CG_DRAW_ACTIVE_FRAME:
		CG_DrawHarness();
		return 0;
	default:
		return 0;
	}
}
