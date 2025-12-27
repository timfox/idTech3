# idTech3 Agent Notes

## How to build
- Vulkan: ./compile_engine.sh vulkan
- OpenGL: ./compile_engine.sh opengl
- Clean: ./compile_engine.sh clean <mode>

## Repo constraints
- Use C23 and C++23 methodologies.
- Lean into C++ usage.
- Renderer boundary is sensitive: avoid changing public interfaces unless required.
- Prefer small diffs and incremental fixes.

## What “done” means
- Builds succeed for the targeted backend.
- No new warnings from obvious compiler output.
- If touching Vulkan: sanity-check swapchain, barriers, and resource lifetimes.
