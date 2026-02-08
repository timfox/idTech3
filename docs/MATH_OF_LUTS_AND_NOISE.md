## Math of LUTs and Noise Workflows

### LUT generation dependencies  
The atmosphere renderer produces its four LUTs in a strict order so later stages can reuse earlier results without recomputation (see Fig. 1 in Sakmary et al.).  
- **Transmittance**: captures attenuation along a line ray and is used by every subsequent LUT.  
- **Multiscattering**: pre-integrates in-scattered light, sampling the computed transmittance to avoid tracing toward the sun for every ray.  
- **Sky-View**: samples both transmittance and multiscattering while mapping the sky into a latitude/longitude texture; the sun is fixed at a known location in the LUT so shading can remain stable as the key view changes.  
- **Aerial Perspective**: stores depth-dependent luminance and transmittance for each slice; distant pixels read this LUT to fade out scene contributions without re-running the full scattering integral.  
Thinking of this chain as a dependency graph simplifies GPU syncing when preparing multiple LUT passes—each compute dispatch simply waits on the prior LUT before emitting its results.

### Worley noise normalization  
Two 4-channel, 16-bit float textures hold the Worley noise used by the volumetric clouds. The shader pipeline is split into two passes: the first pass generates four single-channel Worley noise volumes (one channel per texture); the second pass normalizes and packs those channels into the signed RGBA textures shown in Fig. 5. A single pipeline barrier between the passes ensures writes from the first dispatch are visible before the normalizer reads, so the final textures are guaranteed to be tileable and normalized without re-running the heavy generator.

### Histogram-based key value  
Tonemapping uses a two-pass adaptive luminance scheme:  
1. A compute pass builds a histogram of luminance values sampled from the HDR render target.  
2. A second pass sums the histogram bins to produce the weighted average (key value) used for tonemapping.  
That key value mirrors our auto-exposure pipeline, which also collects luminance, smooths it on the CPU, and pushes the resulting exposure multiplier before running the gamma shader. Aligning the two strategies keeps the adaptive HDR→LDR conversion behavior consistent across our renderer.

### Verifying baked lighting + glints

- Run the renderer with `r_lightmap 1`, `r_tonemap` in a cinematic mode, and `r_autoExposure 1`, then inspect debug views 20/21 to make sure `PBR_BIND_LIGHTMAP` samples the expected map described in step 1.  
- Enable `r_glints 1` (and `glint 1`/`q3map_glint 1` on your materials) and watch the `PBR glints bind` logs in `tr_shade.c`—they should show the dictionary texture/view plus `valid=1`. That log ties directly to the descriptor comment inserted earlier.  
- Compare the final frame to the HDR reference photo with the same exposure key/value settings; when both lightmap and glint paths are valid, the differences should vanish and the scene should lightmatch the target.
- When a stage is missing its own PBR assets the renderer now binds a white fallback descriptor via `vk_get_pbr_fallback_descriptor()` so we never crash; use the fallback log line printed by `vk_get_pbr_descriptor_for_pipeline()` and `vk_bind_descriptor_sets()` to confirm real assets are bound before trusting the HDR comparison.  
