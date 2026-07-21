/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CSS-style UI filter / backdrop-filter blur compositor.

Implements `filter: blur()` (blur an element and its children, rendered into an
offscreen compositor layer) and `backdrop-filter: blur()` (blur only the scene
already rendered behind the element) for the JavaScript/imperative UI.

Design summary:
  - World post-processing and tonemap finish first; the tonemapped swapchain is
    the resolved scene used as the backdrop source (copied once per frame into a
    transient pooled texture, never sampled while being written).
  - Blur runs in linear color space (sRGB decode on ingest, encode on composite)
    to avoid gamma-space halos.
  - Small radii use a separable Gaussian; large radii use a downsampled
    dual-Kawase pyramid. Radii are quantized to bound shader/state permutations.
  - One shared full-screen blurred backdrop is built per frame and reused by all
    backdrop panels; results are clipped through an antialiased rounded-rect mask
    and composited before the HUD overlay so panel text/borders stay sharp.
  - Transient render targets are pooled (allocated once, reused every frame).

Everything is a graceful no-op when the pool/pipelines cannot be created
(ui_blurQuality 0, headless, allocation failure): panels fall back to the plain
translucent overlay path.
===========================================================================
*/
#ifndef VK_UI_BLUR_H
#define VK_UI_BLUR_H

#include "tr_local.h"

/* Lifecycle (called from renderer init/shutdown and vid_restart). */
void vk_ui_blur_init( void );
void vk_ui_blur_shutdown( void );
void vk_ui_blur_register_cvars( void );

/* Per-frame op queue. */
void vk_ui_blur_begin_frame( void );
void vk_ui_blur_enqueue_backdrop( const uiBackdropFilter_t *bf );
void vk_ui_blur_enqueue_layer( const uiCompositorLayer_t *layer );
qboolean vk_ui_blur_has_work( void );

/*
 * Execute all queued ops. Called from the gamma pass after the tonemap draw has
 * written the swapchain and before the HUD overlay compose. Must be called while
 * NOT inside a render pass. `sceneSrc` is the resolved scene view the gamma pass
 * consumed (currently unused for sampling because we copy the tonemapped
 * swapchain, but retained for future HDR-source paths and diagnostics).
 */
void vk_ui_blur_execute( VkImageView sceneSrc );

/* True when ui_blurQuality > 0 and the pool/pipelines are available. */
qboolean vk_ui_blur_available( void );

#endif /* VK_UI_BLUR_H */
