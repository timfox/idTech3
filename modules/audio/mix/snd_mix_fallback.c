/* Fallback implementation for S_WriteLinearBlastStereo16_SSE_x64
 * Provides a weak C implementation that forwards to the portable C writer.
 * If an optimized assembly version is present, the strong symbol will override this.
 *
 * MSVC + USE_WIN32_ASM links snd_mix_x64.asm instead; weak symbols are not
 * supported there, so skip this TU to avoid LNK2005.
 */
#include "../client/client.h"
#include "../snd_local.h"

#if !( defined( _MSC_VER ) && defined( USE_WIN32_ASM ) )

/* Globals used by the portable writer (defined in snd_mix.c) */
extern int *snd_p;
extern int snd_linear_count;
extern short *snd_out;

/* Prototype of the portable writer */
void S_WriteLinearBlastStereo16(void);

#if defined( __GNUC__ ) || defined( __clang__ )
void S_WriteLinearBlastStereo16_SSE_x64(int *p, short *out, int count) __attribute__((weak));
#endif

void S_WriteLinearBlastStereo16_SSE_x64(int *p, short *out, int count)
{
    int *old_p = snd_p;
    short *old_out = snd_out;
    int old_count = snd_linear_count;

    snd_p = p;
    snd_out = out;
    snd_linear_count = count;

    S_WriteLinearBlastStereo16();

    /* restore globals */
    snd_p = old_p;
    snd_out = old_out;
    snd_linear_count = old_count;
}

#endif /* !(MSVC && USE_WIN32_ASM) */
