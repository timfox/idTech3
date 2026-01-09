# id Tech 3++ Development Roadmap

## Core Engine Modernization
- [ ] **OpenAL Audio**: Replace legacy audio system with OpenAL for better spatialization.
- [ ] **Bullet Physics**: Integrate Bullet for rigid body simulation (beyond simple AABB).
- [x] **Networking**: Improved reliability/latency with DDoS protection, deterministic challenges, and enhanced packet security.
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
- [x] **Crash Resilience**: Enhanced memory safety, improved crash handling, and comprehensive error logging.

## Platform & Compatibility
- [x] **Steam Deck**: Enhanced integration with VP9/AV1 video codec support, documented integration improvements.
- [x] **Wayland Support**: Completed implementation plan with touch/gesture support, native clipboard, and multi-monitor compatibility.
- [ ] **Reproducible Builds**: Finalize CI pipelines for all platforms.

## Technical Debt & Stability
- [x] **Deterministic Replays**: Fixed RNG drift in challenge generation, documented floating point determinism improvements.
- [x] **Type Safety Audit**: Added named constants for VM error buffers and magic numbers, improved const correctness.
- [x] **Security**: Implemented comprehensive DDoS protection, enhanced packet parsing security, hardened BSP validation.

## Engine Window Enhancements
- [ ] **Add qt**: Add C++ qt
- [ ] **Upgrade Window security**: Replace X11 with Wayland support

## Refactor
- [ ] **Modernize to C23 and C++23:** Update the game engine code to meet C23 and C++23 standards
- [ ] **Port everything to C++23:** Refactor C code into C++