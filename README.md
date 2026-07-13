# id Tech 3

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE.md) [![Stars](https://img.shields.io/github/stars/timfox/idTech3?style=social)](https://github.com/timfox/idTech3) [![Forks](https://img.shields.io/github/forks/timfox/idTech3?style=social)](https://github.com/timfox/idTech3/forks)

Modernized id Tech 3: **Vulkan renderer with PBR**, validation and smoke-tested builds, and disciplined cross-platform engine work.

*This repository does not contain any game content from Quake III Arena.*

### Engine pillars

1. **Compatibility first** — QVM + native modules, classic `.pk3` / cvar / trap surface ([COMPATIBILITY.md](docs/COMPATIBILITY.md)); `./scripts/q3_openarena_compat_check.sh` on **`core`** builds.
2. **Renderer** - Vulkan 1.x path with PBR, froxel volumetrics, SSAO, MSAA/SMAA, spherical harmonics lighting, SDF HUD text.
3. **Tooling** - GPU detection, validation layers, performance HUD, safe mode, CI matrix builds, smoke tests and shader validation in the build. **[FreeUSD](docs/FREEUSD.md)** Git submodule (`src/external/FreeUSD`, default ON) for USDA import and `usd_*` tools. Optional **[Tiled Map Editor](docs/TILED.md)** submodule (`tools/tiled`, GPL-2.0) for tile-based level design—not linked into the engine build.
4. **Platform** - Linux, Windows, macOS, Android; IPv4/IPv6 networking, modern codecs and asset loaders.

### Features

**Rendering**:
* Vulkan renderer
* Physically Based Rendering (PBR)
* Spherical Harmonics lighting support
* Screen Space Ambient Occlusion (SSAO)
* High-quality SDF HUD text rendering with UTF-8 glyph support
* SVG asset rasterization (NanoSVG / NanoSVRast, built-in; no extra system libs)
* Froxel-based Volumetric Lighting with 2D Navier–Stokes fluid solver
* MSAA and SMAA anti-aliasing

**Audio**:
* OpenAL backend with HRTF for 3D positional audio
* Real-time reverb and occlusion effects via OpenAL EFX (heuristic environmental acoustics)
* Audio codec support: mp3, ogg, wav, flac, webm, opus

**Video**:
* Video codec support: RoQ, WebM (VP8/VP9), Ogg Theora, MP4 (H.264)

**Models**:
* MD3, MDR, IQM, **OBJ**, **glTF / GLB**, MD5, STL, DAE, FBX, **USD / USDA** ([FreeUSD](docs/FREEUSD.md), default ON), Maya Ascii (MA) — loaders under `src/renderers/vulkan/`; glTF details: [docs/GLTF.md](docs/GLTF.md)
* Blend shapes: **IQM** and **glTF** morph + skeletal clips (**`RE_SetEntityMorphWeight`**, **`r_gltfAnim`**, **`r_morph`**, GPU skin/morph via **`r_gltfGpu`**) — [docs/GLTF.md](docs/GLTF.md)

**Renderer (models):** OBJ, glTF/GLB, and MD5 share registration and `MOD_*` types on the Vulkan path. **glTF GPU skin/morph** (`r_gltfGpu`, SSBOs, tangent fix) is Vulkan PBR. See [docs/RENDERERS.md](docs/RENDERERS.md).

**Scripting**:
* Lua
* JavaScript
* Python
* C#

**Images**:
* Image format support: PNG, JPEG, TGA, BMP, DDS, WebP, HDR, OpenEXR (EXR), KTX, PKM, PVR

**Image Generation** (optional):
* FLUX.2/FLUX.1 C image generation (text-to-image from console, real-time hot-reload, device selection; supports flux1-schnell, flux1-dev, flux2-dev; requires model files in game directory; Metal, BLAS, or pure C backend with graceful fallback)

**Networking**:
* Full IPv6 and IPv4 dual-stack support (host, client, server)
* NAT traversal (UDP hole punching with automatic fallback strategies)
* P2P Networking: Supports peer-to-peer server/client connections, NAT traversal, and automatic fallback for robust online sessions without central relays.
* Server discovery: LAN broadcast and master-server queries
* Built-in rate limiting/DDoS mitigation (connection flood protection)
* Challenge/response protocol with replay/spoof resistance enhancements
* Reliable UDP virtual channel (sequenced/reliable message streams)
* Asynchronous non-blocking networking (epoll/kqueue/libsockets abstractions)
* Graceful automatic fallback to IPv4-only if stack or OS configuration requires
* DTLS (Datagram TLS) for secure UDP when enabled at runtime (`net_dtls` + `net_dtls_key`; OpenSSL linked when available at build time)
* Optional TLS-secured remote console (off by default)
* Console/logging for connection diagnostics and real-time network statistics

**Systems** (game and engine extensions):
* AIML 3.0 chatbot scripting (see `g_aiml.c` / Lua `Engine.AIML`)
* Goal Oriented Action Planner (AI planning)
* Flocking and boids (crowd AI)
* hierarchical State Machines (HSM), Finite State Machines (FSM), and Behavior Trees
* Navigation Mesh (NavMesh) pathfinding (A*, Dijkstra)
* Event scripting (Lua, JavaScript)
* Physics (with optional Bullet Physics or Box3D)
* Multithreaded scheduler/task system
* Entity-Component System (ECS, ENTT)
* Replay frame index tied to server snapshots (`engine_replay`; Lua `Engine.Replay`)
* Save slot metadata + `save/engine_slot_*.json` persistence (Lua `Engine.Save`; legacy `.txt` still reads)
* Dynamic dialogue and quest stage tracking (Lua `Engine.Dialogue`, `Engine.Quest`)
* Procedural/random systems
* Statistics / player telemetry counters (Lua `Engine.Telemetry`, cvar `engine_telemetry`; distinct from renderer GPU counters)
* Debugging tools and overlays

### Platforms

* Windows (x64)
* Linux (x64 and arm64)
* Android
* MacOS
* Web

### Standards

* The engine aims to be C++23 and C23-compatible with modernization where it improves safety and portability.
* Native C `bool` is preferred in new or modernized code, but `qboolean` remains in use where needed for backward compatibility.

### Build

Primary entry point:

```bash
./scripts/compile_engine.sh vulkan    # Vulkan + Release
./scripts/compile_engine.sh vulkan debug
```

For direct CMake workflows, the repo also ships `CMakePresets.json`:

```bash
cmake --preset vulkan-release
cmake --build --preset build-vulkan-release
ctest --preset test-vulkan-release
```

Artifacts land under `release/` and `build-vk-Release/`. The helper script is the canonical developer path because it stages `release/` automatically; presets are the preferred direct-CMake / IDE path.

### Editor
The engine features a built-in, cross-platform Editor designed for rapid development and prototyping. The Editor is available as a standalone application and also supports in-game editing modes.

**Key Features:**
- Visual scene and entity editor with real-time viewport rendering.
- Drag-and-drop placement and hierarchical organization of entities and components.
- Support for editing levels, prefabs, lights, navigation meshes, and physics volumes.
- Property inspector for editing component properties, including materials, scripts, and AI behaviors.
- Embedded script editor (Lua and JavaScript) with immediate feedback and live reloading.
- Integration with the engine’s asset pipeline: hot-reload assets, manage resource dependencies, and preview changes instantly.
- Timeline and sequencing tools for cutscenes, events, and gameplay scripting.
- Built-in debugging overlays, profiling tools, and real-time telemetry visualization.
- Multi-user collaborative editing support (via server-client connections).
- Extensible via plugins—extend with custom editors or workflow integrations.

**Platforms:**  
The Editor runs on Windows, Linux, and MacOS.

Refer to the documentation or `scripts/launch_editor.sh` to get started with the desktop Editor.


### Long-term Feature Plans

* Ray tracing (WIP)
* Path tracing
* Voxels

### Links

* https://idtech3.com
* https://github.com/jksunny/quake3e
