# id Tech 3

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE.md) [![Stars](https://img.shields.io/github/stars/timfox/idTech3?style=social)](https://github.com/timfox/idTech3) [![Forks](https://img.shields.io/github/forks/timfox/idTech3?style=social)](https://github.com/timfox/idTech3/forks)

This is a modernized id Tech 3 engine.

*This repository does not contain any game content from Quake III Arena*

### Features

**Rendering**:
* OpenGL renderer
* Vulkan renderer
* Physically Based Rendering (PBR)
* Spherical Harmonics lighting support
* Screen Space Ambient Occlusion (SSAO)
* High-quality SDF HUD text rendering with UTF-8 glyph support
* Optional SVG asset rasterization backend (librsvg/cairo)
* Froxel-based Volumetric Lighting with 2D Navier–Stokes fluid solver
* MSAA and SMAA anti-aliasing

**Audio**:
* OpenAL backend with HRTF for 3D positional audio
* Real-time reverb and occlusion effects via OpenAL EFX (heuristic environmental acoustics)
* Audio codec support: mp3, ogg, wav, flac, webm, opus

**Video**:
* Video codec support: RoQ, WebM (VP8/VP9), Ogg Theora, MP4 (H.264)

**Models**:
* Model formats supported: MD3, MDR, IQM, OBJ, FBX, GLTF, USD (Universal Scene Description), Maya Ascii, Collada (DAE), STL
* Blend Shapes (IQM, GLTF)

**Scripting**:
* Support for Lua scripting
* Support for JavaScript scripting

**Images**:
* Image format support: PNG, JPEG, TGA, BMP, DDS, WebP, HDR, OpenEXR (EXR), KTX, PKM, PVR

**Image Generation**:
* FLUX.2/FLUX.1 C image generation (optional, text-to-image from console with *real-time hot-reloading* and device selection; supports flux1-schnell, flux1-dev, flux2-dev; requires model files in game directory; Metal, BLAS, or pure C backend, with graceful fallback)

**Networking**:
* Full IPv6 and IPv4 dual-stack support (host, client, server)
* NAT traversal (UDP hole punching with automatic fallback strategies)
* Server discovery: LAN broadcast and master-server queries
* Built-in rate limiting/DDoS mitigation (connection flood protection)
* Challenge/response protocol with replay/spoof resistance enhancements
* Reliable UDP virtual channel (sequenced/reliable message streams)
* Asynchronous non-blocking networking (epoll/kqueue/libsockets abstractions)
* Graceful automatic fallback to IPv4-only if stack or OS configuration requires
* DTLS (Datagram TLS) support for secure UDP channels
* Optional TLS-secured remote console (off by default)
* Console/logging for connection diagnostics and real-time network statistics

**Systems**:
* AIML chatbot scripting
* GOAP (AI planning)
* Flocking and boids (crowd AI)
* Finite State Machines (FSM) and Behavior Trees
* Navigation Mesh (NavMesh) pathfinding (A*, Dijkstra)
* Event scripting (Lua, JavaScript)
* Physics and collision (with optional Bullet Physics)
* Multithreaded scheduler/task system
* Entity-Component System (ECS, ENTT)
* Replay and deterministic timing
* Save/restore system
* Dynamic dialogue and quests
* Procedural/random systems
* Statistics/telemetry tracking
* Debugging tools and overlays


### Platforms

* Windows
* Linux
* Android
* MacOS

### Standards

* The engine aims to be C23-compatible, and modernization is ongoing where it improves safety and portability.
* Native C `bool` is preferred in new or modernized code, but `qboolean` remains in use where needed for backward compatibility.

### Links

* https://idtech3.com
* https://github.com/jksunny/quake3e