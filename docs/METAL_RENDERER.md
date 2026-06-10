# Metal / MoltenVK evaluation

Per [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md):

## Tier B (current)

- Ship **MoltenVK** with Vulkan renderer on macOS
- CI: macOS job builds Vulkan client; validate startup + `renderer_regression_check`

## Tier C (`USE_METAL_RENDERER`)

- **`USE_METAL_RENDERER=ON`** (Apple only) builds **`idtech3_metal.dylib`** — roadmap **scaffold** with `GetRefAPI` no-ops (`src/renderers/common/tr_platform_renderer_stub.c`). **Not shippable.**
- `cl_renderer metal` loads the scaffold when present; otherwise falls back to Vulkan.
- Full native Metal backend (`src/renderers/metal/`) only if MoltenVK fails perf targets.
- **Go/no-go**: compare frame time on M-series vs MoltenVK at 1080p with TAA on

### Build scaffold

```bash
cmake -G Ninja -DUSE_METAL_RENDERER=ON ..
cmake --build . --target idtech3_metal
```

## Decision log

| Date | Result |
|------|--------|
| 2026 | MoltenVK default; native Metal deferred |
