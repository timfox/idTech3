# UI Blur — CSS-style `filter` / `backdrop-filter` for the JS UI compositor

Vulkan-only compositor that implements CSS-style blur for the imperative
JavaScript/HUD UI:

```css
filter: blur(8px);
backdrop-filter: blur(18px);
-webkit-backdrop-filter: blur(18px);
```

- `filter: blur()` blurs the element itself (image layer).
- `backdrop-filter: blur()` blurs only the previously rendered scene behind the
  panel; text, borders, and children drawn afterward stay sharp because they go
  through the normal `ui_overlay` path, which is composed **after** the blur.

## Toggle / startup

- Renderer: `ui_blurQuality` (**0** disabled, **1** quarter-res, **2** half-res,
  **3** adaptive — default). Startup log:
  `[VK][ui-blur] CSS filter/backdrop-filter compositor ready (...)`.
- Client: `cl_uiFilter` (default **1**); when 0 or the renderer path is
  unavailable, panels fall back to a plain translucent fill and filter layers
  draw unblurred (graceful fallback, no blur render targets required).

## Frame placement

World post-processing finishes first; the tonemapped swapchain is the resolved
scene texture for backdrop sampling:

```
scene → post chain → gamma/tonemap (writes swapchain)
      → [UI blur: copy swapchain → linearize → blur pyramid → masked composites]
      → overlay compose (HUD/UI text, borders, children — sharp)
      → present
```

The copy means render targets are never sampled while being written (no
framebuffer feedback). Ops are queued through the render command stream
(`RC_UI_FILTER`, `RE_UIBackdropBlur` / `RE_UIFilterLayer` in `refexport_t`) and
executed once per frame in `vk_end_frame_record_gamma_pass`.

## Blur pipeline (renderers/vulkan/vk_ui_blur.c)

- **Transient pooled targets** (`uiTransientTexturePool_t`): one display-format
  scene copy + a 3-level R16G16B16A16_SFLOAT ping-pong pyramid, allocated once
  and rebuilt only when `ui_blurQuality` or the render extent changes
  (dynamic-resolution and `ui_scale` safe). No per-frame texture allocation.
- **Linear color space**: the ingest pass decodes sRGB → linear; blurring
  happens in linear FP16; the composite re-encodes to display space (no
  gamma-space halos).
- **Small radii** (`< ui_blurDownsampleThreshold`, default 12 virtual px):
  separable Gaussian, horizontal + vertical, weights computed in-shader from a
  push-constant tap count (single shader covers every radius bucket).
- **Large radii**: dual-Kawase downsample/upsample over the pyramid
  (`ui_blur_down.frag` / `ui_blur_up.frag`, Bjorge ARM 2015).
- **Radius quantization**: radii snap to 4 px buckets (`ui_blurMaxRadius` cap,
  default 64) so there are no per-radius shader permutations.
- **Shared backdrop pyramid**: all backdrop panels in a frame reuse one blurred
  full-screen pyramid built once from the scene copy — overlapping panels cost
  one extra composite draw each, not a full blur.
- **Static layer cache** (`ui_blurCache`, default 1): a single unchanged
  `filter: blur()` layer skips its blur passes on subsequent frames; the cache
  key covers shader, rect, quantized radius, quality, and extent, and is
  invalidated by any backdrop work or pool rebuild.
- **Rounded-rect mask**: the composite clips through an antialiased
  signed-distance rounded-rectangle evaluated in the panel's rotated local
  frame (border-radius + transforms), with the scissor expanded so the AA
  feather is never clipped.
- **Debug**: `VK_EXT_debug_marker` labels around every pass, per-op CPU record
  timings in `ui_blur_status`, and `ui_filterDebug` visualizations.

## Commands / cvars

| Cvar / command | Meaning |
| --- | --- |
| `ui_blurQuality 0-3` | 0 disabled, 1 quarter-res, 2 half-res, 3 adaptive |
| `ui_blurMaxRadius` | radius cap in virtual (640×480) px |
| `ui_blurDownsampleThreshold` | Gaussian → dual-Kawase switch radius |
| `ui_blurCache` | cache static filtered layers |
| `ui_filterDebug 1` | compositor layer bounds outline |
| `ui_filterDebug 2` | expanded blur bounds outline |
| `ui_filterDebug 3` | backdrop source region (unblurred, masked) |
| `ui_filterDebug 4` | blurred result (mask ignored) |
| `ui_filterDebug 5` | rounded clipping mask (grayscale) |
| `ui_filterDebug 6` | transient texture residency (PiP tiles) |
| `ui_blur_status` | pool residency, per-op timings, cache state |
| `cl_uiFilter` | client-side master toggle / fallback switch |

## Client API (runtime/client/shell/ui_filter.c)

640×480 virtual coordinates, same mapping as `SCR_AdjustFrom640` (aspect,
`ui_scale`):

```c
SCR_UIBackdropBlur( x, y, w, h, radius, cornerRadius, rotation, opacity, tint );
SCR_UIFilterLayer( x, y, w, h, hShader, radius, cornerRadius, rotation, opacity );
UIFilter_ParseChain( "blur(8px)", &chain );   /* CSS value parser */
```

`ui_css.c` stylesheets accept `filter`, `backdrop-filter`,
`-webkit-backdrop-filter` (blur() only), and `border-radius`; values land in
`ui_css_styles_t.filter_blur` / `.backdrop_blur` / `.border_radius`.

## JavaScript (Duktape)

```js
// backdrop-filter: blur() — panel over the scene, text drawn after stays sharp
idtech3.hudBackdropBlur(x, y, w, h, radius
    [, cornerRadius, opacity, tintR, tintG, tintB, tintA, rotation]);

// filter: blur() — the image itself is blurred
idtech3.hudFilterBlurPic(x, y, w, h, shaderOrName, radius
    [, cornerRadius, opacity, rotation]);
```

## Validation scene

`exec demo_ui_blur.cfg` (demo pk3) loads `scripts/js/demo_ui_blur.js`: one
`filter: blur()` image, a backdrop panel with sharp text, a nested blurred
panel, rounded corners, an animated rotated panel, a partially offscreen panel,
and three overlapping panels sharing the backdrop pyramid. Resize/DPI cases
(1080p → 4K, ultrawide, dynamic resolution, `ui_scale`) are covered by the
extent-keyed pool rebuild.

Static wiring test: `tests/scripts/test_ui_blur.sh`.

## Fallbacks

- `ui_blurQuality 0`, missing swapchain `TRANSFER_SRC` support, or any
  pipeline/pool allocation failure → compositor reports unavailable and the
  client draws plain translucent panels (never a hard failure).
- Stub/non-Vulkan renderers leave the `refexport_t` entries NULL; the client
  NULL-checks before use.
