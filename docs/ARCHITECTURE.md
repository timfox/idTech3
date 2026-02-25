# Architecture Overview

## Layer Cake Architecture

The engine follows a three-layer architecture that preserves compatibility while enabling modern features.

### Vanilla Layer (Foundation)
Pure id Tech 3 engine — the original Quake III Arena codebase. APIs and game logic remain untouched to guarantee backward compatibility with all existing mods and maps.

### Chocolate Layer (Enhancement)
Performance and quality improvements that maintain zero breaking changes. Every enhancement has a fallback path and can be disabled completely. Includes the Vulkan renderer, audio codec support, and quality-of-life improvements.

### Layer Cake (Modern Systems)
Modern abstractions and systems built on top of the foundation: PBR materials, volumetric fog, fluid simulation, SMAA, compute shaders, and the modern video codec system.

## Directory Structure

```
src/
├── client/          Client-side systems (input, UI, cinematics, networking)
│   ├── cl_cin.c              Legacy ROQ cinematic decoder
│   ├── cl_cin_modern.c/h     Modern video codec dispatcher
│   ├── cl_cin_ffmpeg.c       FFmpeg backend (H.264/H.265/VP9/AV1)
│   ├── cl_cin_dav1d.c        dav1d AV1 backend
│   ├── cl_cin_vpx.c          libvpx VP8/VP9 backend
│   └── cl_cin_theora.c       Theora backend
├── server/          Server-side game logic and networking
├── qcommon/         Shared engine utilities, math, filesystem, VM
├── audio/           Audio subsystem
│   ├── backends/    OpenAL, SDL, and null audio backends
│   ├── codecs/      WAV, MP3, Opus, FLAC, WebM audio decoders
│   ├── effects/     EFX reverb and spatial audio
│   └── mix/         Audio mixing and DMA
├── renderers/
│   ├── openglrenderer/   OpenGL renderer (fallback)
│   ├── vulkanrenderer/   Vulkan 1.4 renderer (primary)
│   │   ├── vk.c/h              Core Vulkan pipeline and dispatch
│   │   ├── vk_vfog.c/h         Volumetric fog module (cvars + params)
│   │   ├── vk_fluidsim.c/h     Navier-Stokes fluid simulation module
│   │   ├── vk_vbo.c            Vertex buffer objects
│   │   ├── vk_flares.c         Lens flare system
│   │   ├── vk_mikktspace.c     Tangent space generation
│   │   ├── tr_*.c              Scene, shader, model, BSP loading
│   │   └── shaders/glsl/       GLSL shader sources
│   │       ├── gen_vert.tmpl    General vertex shader template
│   │       ├── gen_frag.tmpl    General fragment shader template
│   │       ├── gamma.frag       Post-processing (tonemap, bloom knee)
│   │       ├── smaa_*.frag      SMAA anti-aliasing passes
│   │       └── volumetric/      Volumetric fog + fluid compute shaders
│   └── rendercommon/     Shared renderer utilities
├── platform/
│   ├── unix/        Linux/macOS platform layer
│   ├── win32/       Windows platform layer
│   └── sdl/         SDL2 windowing, input, gamma
├── botlib/          Bot AI library
└── external/        Vendored third-party libraries
    ├── src/         Source libraries (opus, flac, ogg, zlib, libpng, etc.)
    ├── include/     Header-only libraries (entt, glm)
    └── duktape/     Duktape JavaScript engine
```

## Renderer Architecture

Both renderers implement the same `refexport_t` interface and are loaded as shared libraries via `dlopen` (Linux/macOS) or statically linked (Windows).

### Vulkan Renderer Pipeline
1. **Scene submission** — Game code submits entities, lights, and surfaces
2. **BSP traversal** — Visible surfaces determined via PVS and frustum culling
3. **Shadow passes** — Sun CSM, spot shadow atlas, point shadow cubemaps
4. **Main render pass** — Geometry with PBR materials, per-pixel lighting
5. **Volumetric fog** — Froxel compute scatter + temporal integration
6. **SSAO** — Screen-space ambient occlusion with blur
7. **Bloom** — HDR extraction + multi-pass Gaussian blur + blend
8. **SMAA** — Sub-pixel morphological anti-aliasing (edge + blend + compose)
9. **Post-processing** — Tonemapping (ACES/Reinhard), gamma, panini projection
10. **Present** — Swapchain presentation

## Build System

CMake 3.24+ with optional Ninja backend. See [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) for build instructions.

## Scripting

Optional Lua 5.1+ and Duktape (JavaScript) scripting engines for gameplay logic, enabled via `USE_LUA` and `USE_DUKTAPE` CMake options.
