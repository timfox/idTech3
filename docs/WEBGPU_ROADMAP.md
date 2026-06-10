# WebGPU / WebAssembly roadmap

Browser deployment is **not shipping**. This document tracks the portable-renderer path and shader sources that align with WebGPU constraints.

## Status

| Target | Status |
|--------|--------|
| **Native Vulkan** | Shipping (`cl_renderer vulkan`) |
| **WebGPU in browser** | Roadmap — Emscripten + Dawn/`emdawnwebgpu` |
| **`cl_renderer webgpu`** | **Rejected at client init** — Wasm-only; falls back to Vulkan with `docs/WEBGPU_ROADMAP.md` hint |

There is no `idtech3_webgpu` native plugin. WebGPU is a **Wasm render surface**, not a desktop dlopen backend.

## Portable shader families (Vulkan today)

These compute paths are written for **tile-friendly** workgroups and bounded memory — intentional stepping stones toward WGSL:

| Family | Cvar | GLSL directory | Notes |
|--------|------|----------------|-------|
| **WebSplatter** | `r_wsp` | `src/renderers/vulkan/shaders/glsl/wsp/` | 16×16 tiles, atomics, composite |
| **Mobile-GS** | `r_mgs` | `src/renderers/vulkan/shaders/glsl/mgs/` | Tiered splat budgets |

Run the portability manifest check:

```bash
./scripts/check_webgpu_shader_portability.sh
```

## Planned phases

1. **Manifest + CI** — list portable `.comp` shaders; block accidental desktop-only extensions in those paths.
2. **WGSL export** — `scripts/export_wgsl_from_glsl.sh` (SPIR-V → naga/wgsl or hand port for `wsp/*`).
3. **Emscripten client** — separate CMake toolchain; `IMGUI_IMPL_WEBGPU` reference in `src/external/src/cimgui/imgui/examples/example_sdl3_wgpu/`.
4. **Asset budget** — reuse Mobile-GS / WebSplatter caps for WebGPU memory limits.

## References

- [WEB_SPLATTER.md](WEB_SPLATTER.md)
- [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
