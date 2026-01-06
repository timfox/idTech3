# id Tech 3++ Development Roadmap

## Core Engine Modernization
- [ ] **OpenAL Audio**: Replace legacy audio system with OpenAL for better spatialization.
- [ ] **Bullet Physics**: Integrate Bullet for rigid body simulation (beyond simple AABB).
- [x] **Networking**: Improve reliability/latency (Added showpackets fragment debugging).
- [x] **Lua Scripting**: Complete the engine-side Lua runtime and entity bindings.
- [ ] **OOP Entity Architecture**: Move beyond legacy `gentity_t` to a modern C++ class hierarchy.

## Rendering Improvements
- [x] **Ray Tracing (Hardware)**: Optimize BVH builds and pipeline for Vulkan/DXR - Path tracing implemented with r_rt_pathtracing CVAR.
- [ ] **Mesh Shaders**: Implementation for modern GPU geometry pipelines.
- [ ] **Material System**: Layered materials and procedural shader support.
- [x] **Dynamic Resolution**: Runtime resolution scaling for performance stability.
- [ ] **Bench enhancement**: Add per-iteration timings, memory metrics, and time-series dashboards; include parsing tools and CI publishing for bench_timeseries.jsonl and bench.csv.
- [ ] **Improve Image formats**: Support RGB9E5 32-bit pixel format for storing HDR (High Dynamic Range) color data
- [x] **NVIDIA Vulkan Compatibility**: NVIDIA GeForce RTX 3060 now successfully initializes and runs Vulkan renderer with automatic driver compatibility workarounds

## Filesystem & Assets
- [ ] **Virtual FS v2**: Improved mount table and priority system.
- [ ] **Asset Pipeline**: Automated asset cooking (KTX2, BasisU) and validation.
- [ ] **Shader Pipeline**: Versioning and better hot-reload support.

## Developer Experience & Tooling
- [ ] **Radiant Editor**: Focus on stability, undo/redo, and modern entity inspection.
- [x] **In-game Debugging**: Expand ImGui overlays for deeper subsystem inspection.
- [x] **Crash Resilience**: Improved "Safe mode" boot connected to crash handler.

## Platform & Compatibility
- [ ] **Steam Deck**: Controller glyphs and seamless virtual keyboard integration.
- [ ] **Wayland Support**: Native Wayland backend for Linux.
- [ ] **Reproducible Builds**: Finalize CI pipelines for all platforms.

## Technical Debt & Stability
- [ ] **Deterministic Replays**: Fix floating point and RNG drift for network sync.
- [ ] **Type Safety Audit**: Continue converting legacy `void*` and `char*` to safer types.
- [ ] **Security**: Continue hardening BSP and network packet parsing.

## Engine Window Enhancements
- [ ] **Add qt**: Add C++ qt
- [ ] **Upgrade Window security**: Replace X11 with Wayland support

## Refactor
- [ ] **Modernize to C23 and C++23:** Update the game engine code to meet C23 and C++23 standards
- [ ] **Port everything to C++23:** Refactor C code into C++