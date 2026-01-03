/* Removed duplicate block above; keep a single definition elsewhere. */

/*
 * Centralized SDF font CVAR definitions to avoid symbol duplication
 */

#include "tr_public.h"

// Global SDF-related font CVARs
cvar_t *r_fontSDF = NULL;
cvar_t *r_fontSDFSpread = NULL;
cvar_t *r_fontSDFSmooth = NULL;
cvar_t *r_fontSDFOutline = NULL;

// Initialize all SDF CVARs with their baseline definitions
void TR_Init_FontSDF_CVARS(void) {
    r_fontSDF = Cvar_Get( "r_fontSDF", "0", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_CheckRange( r_fontSDF, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_fontSDF, "Use signed distance field (SDF) font atlases when available. Requires atlas rebuild." );

    r_fontSDFSpread = Cvar_Get( "r_fontSDFSpread", "6", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_CheckRange( r_fontSDFSpread, "4", "16", CV_INTEGER );
    ri.Cvar_SetDescription( r_fontSDFSpread, "SDF spread (pixels) for SDF font baking." );

    r_fontSDFSmooth = Cvar_Get( "r_fontSDFSmooth", "0.25", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_CheckRange( r_fontSDFSmooth, "0.05", "0.5", CV_FLOAT );
    ri.Cvar_SetDescription( r_fontSDFSmooth, "SDF smoothstep width (normalized distance). Higher = softer edges." );

    r_fontSDFOutline = Cvar_Get( "r_fontSDFOutline", "0", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_CheckRange( r_fontSDFOutline, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_fontSDFOutline, "Enable SDF font outline/glow effects for better text readability." );
}
