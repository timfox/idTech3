## Vulkan PBR Glint Tutorial

This document explains how idTech3’s Vulkan renderer exposes the Chermain 2020 microfacet **glint** subsystem within the PBR pipeline. It is intended for engine integrators and shader authors who want to understand how the dictionary, GPU resources, and cvars work together.

### 1. High-level architecture

- **Dictionary generation**: `src/renderers/vulkanrenderer/glints.c` builds a hierarchical dictionary of Beckmann-like distributions per entry/level/size. Modes 0/1 emit legacy lobed dictionaries and mode 2 is the Chermain stochastic sampler.
- **Vulkan integration**: `vk_build_glint_dictionary()` allocates the CPU buffer, uploads it via `vk_create_glint_dictionary_texture()`, and exposes the texture through descriptor bindings (`vk_fill_pbr_image_info`, `vk_update_pbr_indexed_glint`, `vk_update_glint_descriptor_binding`). `vk_destroy_glint_dictionary_cpu()` guards against double frees.
- **Shader usage**: `shaders/glsl/gen_frag.tmpl` samples `glint_dict_texture` with helper `glint_dictionary_lookup()` and adds the result into the specular term. The glint parameters are passed from `tr_shade.c` (`VK_SetGlintParams`) through the uniform buffer so the shader can interpolate noise, energy compensation, and masking (G term).

### 2. Enabling & tuning

1. **Enable the system**: `USE_VK_PBR` is defaulted (`tr_local.h`), so the renderer already compiles the glint pipeline. Make sure you launch the Vulkan renderer (`r_renderer = vulkan`) and `r_glints` cvars exist.
2. **Key cvars** (see `tr_init.c` for defaults):
   - `r_glints`: master switch (`0` to disable, `1` for default). This also gate-keeps the uniform updates.
   - `r_glints_mode`: dictionary algorithm (0 legacy, 1 legacy + Chermain mix, 2 Chermain).
   - `r_glints_entries`, `r_glints_levels`, `r_glints_size`, `r_glints_alpha`, `r_glints_lobeSigma`: dictionary layout parameters. Entries × levels form the texture height; size is width. Alpha controls base sigma, lobeSigma sets lobe width.
   - Quality/debug toggles: `r_glints_budget`, `r_glints_strength`, `r_glints_debug`, `r_glints_verbose`, etc.
3. **Tuning workflow**:
   - Adjust `r_glints_*` cvars and call `vid_restart` or toggle `r_glints_forceReload` to rebuild the dictionary.
   - Monitor console output: verbose mode prints build stats (entries, size, sample count, duration). Shader debug modes visualize masks (`r_glints_debug`), energy, or dictionary slices.

### 3. Runtime lifecycle

1. **Initialization**: `vk_initialize()` zeroes `vk.glint` and later calls `vk_update_glint_dictionary_if_needed()`.
2. **Per-frame updates**: `tr_shade.c` calls `vk_update_glint_dictionary_if_needed()` before drawing so atlas parameters stay synchronized. Uniform buffer `vkUniform_t` fills the glint vec4 slots (core/material/micro/temporal/energy/routing/color).
3. **Resource destruction**: `vk_shutdown()` and `vk_release_resources()` call `vk_destroy_glint_dictionary_texture()` and `vk_destroy_glint_dictionary_cpu()`. The CPU helper now tracks `cpu_allocated` to avoid double frees while still releasing memory across `vk_update_glint_dictionary_if_needed()` rebuilds.

### 4. Debugging tips

- **Descriptor availability**: If glint sampling fails acquire a fallback texture (white/black); check `vk_get_glint_dictionary_view()` and `vk_glint_dictionary_image`.
- **Performance knobs**: Smaller dictionaries cost less upload time; drop entries/levels and rely on shader blending for high-frequency at higher mip levels.
- **Energy balancing**: `r_glints_energyBoost` and `r_glints_normalScale` (if exposed) keep total specular energy consistent, so increase one while adjusting base specular (metallic/roughness) to avoid fireflies.
- **Shader inspection**: `gen_frag.tmpl` contains `ComputeGlintContribution`, `ComputeGlintSlope`, and dictionary lookup macros—look at the binding comments near line 180 for sampler layout.

### 5. Building shaders

Use `scripts/compile_vulkan_shaders.py` (or `shaders/tools/compile_threaded.cpp`) to regenerate SPIR-V whenever you modify the GLSL templates. The compiled blobs live under `src/renderers/vulkanrenderer/shaders/spirv/` and are referenced in `shader_binding.c`. The glint sampler always resides at `set=5, binding=11`.

### 6. Summary

The Vulkan glint system is production-ready: dictionary generation, descriptor binding, shader sampling, and cvars are already wired into the renderer. Use this guide to customize the dictionary parameters and debug the interaction between GPU resources and shader code when refining microfacet glints.
