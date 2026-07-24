# World Presentation — Provenance Log

Each feature records clean-room provenance. Update when implementation advances.

---

### Feature: HDR eye adaptation / exposure volumes

- **Observable behavior:** Asymmetric histogram auto-exposure; volume blend of EV settings.
- **Independent mathematical basis:** Log2 luminance histogram; percentile clip; exponential adaptation \(x += (t-x)(1-e^{-rt})\).
- **Public references:** HDR imaging / eye adaptation literature (e.g. Krawczyk et al. style discussions), renderer-local SceneHDR docs.
- **Original implementation design:** `luminance.comp`, `vk_temporal.c`, `vk_exposure_volumes.c`, `vk_hdr_sun.c`.
- **Original test assets:** Surf `surf_aztec` HDR sky; lab sequences in tests.
- **Files changed:** see git for module list.
- **Compatibility limitations:** Map volumes cannot apply exposure per-pass independently.

### Feature: Sky environment

- **Observable behavior:** Scaled secondary world behind main opaque scene.
- **Independent mathematical basis:** Camera origin remapped as \(o' = e + (o-e)/s\).
- **Public references:** Layered sky / portal sky presentations in public engine docs and graphics talks.
- **Original implementation design:** `vk_sky_environment.c` (scaffold → composition).
- **Original test assets:** Lab skyline scene (planned).
- **Files changed:** `vk_sky_environment.c/h`.
- **Compatibility limitations:** No gameplay collision; separate visibility.

### Feature: Environment reflection probes

- **Observable behavior:** Local specular/diffuse environment sampling with blend.
- **Independent mathematical basis:** Split-sum IBL; box parallax projection.
- **Public references:** Karis 2013 IBL; McGuire parallax-corrected cubemaps.
- **Original implementation design:** `vk_environment_probes.c` (+ existing probe GI where applicable).
- **Original test assets:** Box/sphere probe lab.
- **Files changed:** `vk_environment_probes.c/h`.
- **Compatibility limitations:** No recursive probe capture in gameplay.

### Feature: Reflective material extension

- **Observable behavior:** Masked Fresnel reflections into SceneHDR.
- **Independent mathematical basis:** Schlick Fresnel; roughness mip selection.
- **Public references:** PBR textbooks / Filament docs (public).
- **Original implementation design:** `reflectionMaterialExtension_t` in world presentation materials.
- **Original test assets:** Metallic/roughness ladder.
- **Files changed:** material extension + docs.
- **Compatibility limitations:** Artistic mode energy-bounded.

### Feature: Water

- **Observable behavior:** Reflection/refraction/absorption/foam.
- **Independent mathematical basis:** Fresnel; Beer–Lambert absorption.
- **Public references:** Public water rendering articles (e.g. GPU Gems-class techniques).
- **Original implementation design:** Dedicated water route, quality cvars.
- **Original test assets:** Shallow/deep water lab.
- **Files changed:** `vk_water_presentation.c/h`.
- **Compatibility limitations:** Not ordinary WBOIT; batched reflection sources.

### Feature: Projected lights

- **Observable behavior:** Cookie + depth shadow projected lights / flashlight.
- **Independent mathematical basis:** Perspective projection; reversed-Z bias.
- **Public references:** Shadow mapping surveys; clustered shading papers.
- **Original implementation design:** `vk_projected_lights.c` + Forward+ clusters.
- **Original test assets:** Moving flashlight lab.
- **Files changed:** `vk_projected_lights.c/h`.
- **Compatibility limitations:** Capacity bounded.

### Feature: Decals

- **Observable behavior:** Surface marks without sky/weapon bleed.
- **Independent mathematical basis:** Deferred projection via reconstructed position.
- **Public references:** Deferred decal techniques (public presentations).
- **Original implementation design:** Geometry + `vk_deferred_decals.c`.
- **Original test assets:** Corner/wall decal lab.
- **Files changed:** decal modules.
- **Compatibility limitations:** Generation-tied G-buffer.

### Feature: Detail layers / lightstyles / local fog / color volumes / drivers / portals / terrain / viewmodel

- Scaffolded under world presentation ownership with per-feature docs.
- Mathematics and references recorded as each path leaves stub certification.

### Naming note

Runtime commands and code comments use generic terms (`world_*`, `environment probe`,
`sky environment`). Provenance may discuss public behavioral goals without embedding
third-party product names in console commands.
