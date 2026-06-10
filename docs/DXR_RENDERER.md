# DXR / DirectX 12 renderer (roadmap)

Per [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) and [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).

## Status

| Tier | What ships today |
|------|------------------|
| **A** | **Vulkan** on Windows (`idtech3_vulkan.dll`) — only shipping renderer |
| **B** | Vulkan RTX via `USE_VULKAN_RTX=ON` + `r_rtx` demo path |
| **C** | **`USE_DXR_RENDERER=ON`** builds **`idtech3_dxr.dll`** — roadmap **scaffold only** |

The DXR plugin exports `GetRefAPI` with safe no-op `RE_*` entry points. Selecting `cl_renderer dxr` loads the scaffold when the DLL is present; otherwise the client falls back to Vulkan.

## Build (Windows only)

```bash
cmake -G Ninja -DUSE_DXR_RENDERER=ON ..
cmake --build . --target idtech3_dxr
```

Startup log when the scaffold loads:

```
[DXR] roadmap renderer scaffold (USE_DXR_RENDERER=ON) — not shippable; use cl_renderer vulkan. See docs/DXR_RENDERER.md
```

## Planned implementation

- Module: `src/renderers/dxr/` (future; not started)
- API: DirectX 12 + DXR 1.1 (`ID3D12Device5`, state objects, SBT)
- Shared: `tr_image.c`, `tr_bsp.c`, model loaders; new HLSL pipeline + swapchain
- RT: mirror Vulkan RTX phases (BLAS/TLAS, hybrid raster + trace)

## Decision log

| Date | Result |
|------|--------|
| 2026 | DXR deferred behind Vulkan; scaffold plugin for dlopen seam validation |
