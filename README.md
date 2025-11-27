# id Tech 3

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) * <a href="https://discord.com/invite/X3Exs4C"><img src="https://img.shields.io/discord/314456230649135105?color=7289da&logo=discord&logoColor=white" alt="Discord server" /></a>

This is a modern id Tech 3 engine with PBR and ray tracing.

Go to [Releases](../../releases) section to download latest binaries for your platform or follow [Build Instructions](#build-instructions)

*This repository does not contain any game content from Quake III Arena*

**Key features**:

* Optimized OpenGL renderer
* Optimized Vulkan renderer
* Vulkan ray tracing
* Physical Based Rendering
* Raw mouse input support, enabled automatically instead of DirectInput(**\in_mouse 1**) if available
* Unlagged mouse events processing, can be reverted by setting **\in_lagged 1**
* **\in_minimize** - hotkey for minimize/restore main window (win32-only, direct replacement for Q3Minimizer)
* **\video-pipe** - to use external ffmpeg binary as an encoder for better quality and smaller output files
* Significally reworked QVM (Quake Virtual Machine)
* Improved server-side DoS protection, much reduced memory usage
* Raised filesystem limits (up to 20,000 maps can be handled in a single directory)
* Reworked Zone memory allocator, no more out-of-memory errors
* Non-intrusive support for SDL2 backend (video, audio, input), selectable at compile time

## Vulkan renderer

* Ray tracing (NEW)
* High-quality per-pixel dynamic lighting
* Very fast flares (**\r_flares 1**)
* Anisotropic filtering (**\r_ext_texture_filter_anisotropic**)
* Greatly reduced API overhead (call/dispatch ratio)
* Flexible vertex buffer memory management to allow loading huge maps
* Multiple command buffers to reduce processing bottlenecks
* [reversed depth buffer](https://developer.nvidia.com/content/depth-precision-visualized) to eliminate z-fighting on big maps
* Merged lightmaps (atlases)
* Multitexturing optimizations
* Static world surfaces cached in VBO (**\r_vbo 1**)
* Useful debug markers for tools like [RenderDoc](https://renderdoc.org/)
* Fixed framebuffer corruption on some Intel iGPUs
* Offscreen rendering, enabled with **\r_fbo 1**, all following requires it enabled:
* `screenMap` texture rendering - to create realistic environment reflections
* Multisample anti-aliasing (**\r_ext_multisample**)
* Supersample anti-aliasing (**\r_ext_supersample**)
* Per-window gamma-correction which is important for screen-capture tools like OBS
* You can minimize game window any time during **\video**|**\video-pipe** recording
* High dynamic range render targets (**\r_hdr 1**) to avoid color banding
* Bloom post-processing effect
* Arbitrary resolution rendering
* Greyscale mode

## OpenGL renderer

* OpenGL 1.1 compatible, uses features from newer versions whenever available
* High-quality per-pixel dynamic lighting, can be triggered by **\r_dlightMode** cvar
* Merged lightmaps (atlases)
* Static world surfaces cached in VBO (**\r_vbo 1**)
* All set of offscreen rendering features mentioned in Vulkan renderer, plus:
* Bloom reflection post-processing effect

Performance is usually greater or equal to other opengl1 renderers

## [Build Instructions](docs/BUILD.md)

## Links

* https://bitbucket.org/CPMADevs/cnq3
* https://github.com/ioquake/ioq3
* https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition
* https://github.com/OpenArena/engine

