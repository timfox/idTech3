#ifndef VK_VELOCITY_SPACE_H
#define VK_VELOCITY_SPACE_H

/*
 * Canonical renderer-wide velocity (motion vector) convention.
 *
 * SPACE:     VK_VELOCITY_SPACE_UV — normalized texture coordinates in [0,1]
 *            at the *render target* resolution (vk_get_render_target_width/height,
 *            i.e. the FBO / r_renderScale extent, NOT the swapchain extent).
 *
 * SIGN:      velocityUV = currentUV - previousUV
 *            (gen_frag.tmpl / light_frag.tmpl: out_motion = currUV - prevUV)
 *
 * CONSUME:   historyUV  = currentUV - velocityUV      == previousUV
 *            (taa.frag / weapon_taa.frag: historyUV = sampleUV - motion)
 *
 * GENERATE:  vec2 currentUV  = (currentClip.xy  / currentClip.w)  * 0.5 + 0.5;
 *            vec2 previousUV = (previousClip.xy / previousClip.w) * 0.5 + 0.5;
 *            out_motion = currentUV - previousUV;
 *
 * Pixel-space velocity is DERIVED only, never stored:
 *            velocityPixels = velocityUV * renderExtent;
 * and must be tagged with the extent it refers to. Never reinterpret
 * render-resolution pixels as output/display-resolution pixels.
 *
 * NDC-space velocity (previousNdc - currentNdc) is exactly 2x the UV
 * velocity. Any producer or consumer that skips the * 0.5 NDC→UV factor
 * introduces a 2x reprojection error (4x if doubled on both ends).
 *
 * INVALID:   out_motion = vec2(0.0/0.0) (NaN) marks "no reliable previous
 *            transform"; consumers must reject history for NaN/Inf motion.
 *
 * JITTER:    Motion vectors must not contain the temporal jitter delta.
 *            Producers cancel jitter by rebasing the previous projection
 *            onto the current frame's jitter (see vk_get_prev_mvp_transform).
 *            TAA removes the current jitter from its sample position only
 *            (sampleUV = uv - jitterPixels * texel).
 *
 * Every velocity producer and consumer must declare its expected space by
 * referencing one of the enum values below (compile-time documentation +
 * runtime report via r_temporalResolutionDebug / temporal_resolution_status).
 */
typedef enum {
	VK_VELOCITY_SPACE_UV = 0,     /* normalized [0,1] render-target UV (canonical) */
	VK_VELOCITY_SPACE_PIXELS = 1, /* pixels at a tagged extent (derived only) */
	VK_VELOCITY_SPACE_NDC = 2     /* [-1,1] clip/NDC delta (never stored) */
} vkVelocitySpace_t;

#define VK_VELOCITY_SPACE_CANONICAL VK_VELOCITY_SPACE_UV

/* Declared spaces for each producer / consumer stage (single source of truth
 * for the r_temporalResolutionDebug report; a mismatch here is a build bug). */
#define VK_VELOCITY_PRODUCER_GEN_FRAG      VK_VELOCITY_SPACE_UV /* gen_frag.tmpl out_motion */
#define VK_VELOCITY_PRODUCER_LIGHT_FRAG    VK_VELOCITY_SPACE_UV /* light_frag.tmpl out_motion */
#define VK_VELOCITY_PRODUCER_FOG_FRAG      VK_VELOCITY_SPACE_UV /* fog.frag / color.frag out_motion */
#define VK_VELOCITY_CONSUMER_TAA           VK_VELOCITY_SPACE_UV /* taa.frag historyUV = sampleUV - motion */
#define VK_VELOCITY_CONSUMER_WEAPON_TAA    VK_VELOCITY_SPACE_UV /* weapon_taa.frag */
#define VK_VELOCITY_CONSUMER_MOTION_BLUR   VK_VELOCITY_SPACE_UV /* gamma.frag matrix-derived */

const char *vk_velocity_space_name( vkVelocitySpace_t space );

/* Runtime extent + convention report (Phase 3/4).
 * force=qtrue prints unconditionally (console command); otherwise only when
 * r_temporalResolutionDebug is set and an extent changed since the last report. */
void vk_temporal_resolution_report( qboolean force );

/* CPU-side reprojection probe (Phase 2/10): projects a fixed view-space point
 * with the current and previous frame matrices, reconstructs the pixel
 * displacement through the canonical UV convention, and warns when the
 * error ratio is ~2x / 4x / 0.5x / 0.25x. Runs when r_temporalVelocityProbe > 0. */
void vk_temporal_velocity_probe( void );

#endif
