# Bloom Pyramid

Extract → optional blit downsample → 5-tap Gaussian H/V (`blur.frag`) × 4 levels → additive blend (`blend.frag`).

Commands: `bloom_pyramid_status` · `bloom_filter_status` · `r_bloomDebug` 1–10.

See [BLOOM_FIREFLY_CONTROL.md](BLOOM_FIREFLY_CONTROL.md).
