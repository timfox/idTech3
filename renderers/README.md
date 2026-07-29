# Renderers

Canonical renderer sources. Vulkan is the shipping backend; platform scaffolds
for Metal/DXR remain optional/non-shipping.

| Path | Role |
|------|------|
| `renderers/vulkan/` | Core Forward+, deferred, BSP, volumetrics |
| `renderers/vulkan/diagnostics/` | Init/status/capture diagnostics included by `tr_init.c` |
| `renderers/vulkan/extensions/` | Neural, splats, RTX, scaffold (profile-gated) |
| `renderers/vulkan/inspector/` | ImGui debug UI |
| `renderers/common/` | Shared font/stub code |

CMake: `IDTECH3_DIR_RENDERERS` in [`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake).
