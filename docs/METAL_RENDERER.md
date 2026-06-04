# Metal / MoltenVK evaluation

Per [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md):

## Tier B (current)

- Ship **MoltenVK** with Vulkan renderer on macOS
- CI: macOS job builds Vulkan client; validate startup + `renderer_regression_check`

## Tier C (`USE_METAL_RENDERER`)

- CMake option currently **stub** — native Metal backend only if MoltenVK fails perf targets
- **Go/no-go**: compare frame time on M-series vs MoltenVK at 1080p with TAA on

## Decision log

| Date | Result |
|------|--------|
| 2026 | MoltenVK default; native Metal deferred |
