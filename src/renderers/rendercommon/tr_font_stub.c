#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_public.h"

#if !defined(BUILD_FREETYPE)

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
void R_InitFreeType(void);
void R_DoneFreeType(void);

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	(void)fontName;
	(void)pointSize;
	(void)font;
	ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType code not available\n");
}

void R_InitFreeType(void) {
	ri.Printf(PRINT_WARNING, "R_InitFreeType: FreeType support is disabled in this build.\n");
}

void R_DoneFreeType(void) {
	ri.Printf(PRINT_WARNING, "R_DoneFreeType: FreeType support is disabled in this build.\n");
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif
