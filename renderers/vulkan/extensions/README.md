# Vulkan renderer extensions (2026)

Opt-in experimental renderer code. CMake gate: **`USE_EXPERIMENTAL_RENDERERS`** (OFF in `game`/`core` profiles).

| Folder | Contents |
|--------|----------|
| `neural/` | NIV, NIST, NVC, VFGI, NDGI, NSLM, RenderFormer, WPT, `vk_neural_io` |
| `splats/` | VkSplat, Mobile-GS, WebSplatter, SqueezeMe |
| `rtx/` | RTX core (`vk_rtx*`), Hybrid1, path trace, GRTX, raygun, FSA |
| `scaffold/` | CuRast, Mímir, Iris, VUDA, Dressi paper scaffolds |

Shipping core stays in `src/renderers/vulkan/` (Forward+, deferred, BSP, volumetrics).

When experimental renderers are OFF, `vk_experimental_renderer_stubs.c` at the vulkan root supplies no-op symbols.

Manifest: [`cmake/renderers/VulkanExtensionSources.cmake`](../../../../cmake/renderers/VulkanExtensionSources.cmake).
