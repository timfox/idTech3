# Bloom Firefly Control

**Shader:** `shaders/glsl/bloom.frag` (extract only — SceneHDR unchanged)  
**Cvars:** `r_bloomFireflyClamp` (default 1), `r_bloomFireflyRatio`, `r_bloomFireflyAbsolute`, `r_bloomFireflyNeighborhood`, `r_bloomFireflyDebug`

Robust neighborhood luminance (cross / 3×3 median / trimmed mean) clamps isolated spikes before soft-knee threshold. Coherent emissive regions survive. Debug 1–6: source / local ref / fireflies / clamped / removed / threshold input.

Does **not** blur SceneHDR. Do not raise global roughness to hide fireflies.
