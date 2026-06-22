# Renderers

2026 layout alias: `renderers/` → `src/renderers/` (Vulkan shipping backend).

| Path | Role |
|------|------|
| `renderers/vulkan/` | Core Forward+, deferred, BSP, volumetrics |
| `renderers/vulkan/extensions/` | Neural, splats, RTX, scaffold (profile-gated) |
| `renderers/vulkan/inspector/` | ImGui debug UI |
| `renderers/common/` | Shared font/stub code |

CMake: `IDTECH3_DIR_RENDERERS` in [`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake).
