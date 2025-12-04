# id Tech 3

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) <a href="https://discord.com/invite/X3Exs4C"><img src="https://img.shields.io/discord/314456230649135105?color=7289da&logo=discord&logoColor=white" alt="Discord server" /></a>

This is a modern id Tech 3 engine with PBR and ray tracing.

Go to [Releases](../../releases) section to download the latest binaries for your platform or follow [Build Instructions](#build-instructions).

*This repository does not contain any game content from Quake III Arena.*

**Key features**:

* OpenGL renderer
* DirectX 12 renderer
* Vulkan renderer
* Vulkan ray tracing
* Metal renderer (macOS/iOS)
* Physically Based Rendering (PBR)
* Material clearcoat, anisotropy, and subsurface scattering options
* Steamworks and Steam Deck compatible
* Modernized graphics options menu
* ImGui layer for in-engine tools and overlays
* SysCall Registry


## Vulkan renderer

* Ray tracing (hardware-accelerated where available, DXR-compatible)
* High-quality per-pixel dynamic lighting
* Surfel-based indirect lighting and global illumination (**experimental**)
* Volumetric fog and light shafts
* Physically correct area and spot lights
* Very fast flares (**\r_flares 1**)
* Anisotropic filtering (**\r_ext_texture_filter_anisotropic**)
* Greatly reduced API overhead (call/dispatch ratio)
* Flexible vertex buffer memory management to allow loading huge maps
* Pipeline cache and incremental pipeline compilation for faster loading and less stutter
* Multiple command buffers to reduce processing bottlenecks
* [Reversed depth buffer](https://developer.nvidia.com/content/depth-precision-visualized) eliminates z-fighting on big maps
* Merged lightmaps (atlases)
* Multitexturing optimizations
* Static world surfaces cached in VBO (**\r_vbo 1**)
* Useful debug markers for [RenderDoc](https://renderdoc.org/)
* Fixed framebuffer corruption on some Intel iGPUs
* Robust device lost & swapchain recreation handling
* Offscreen rendering, enabled with **\r_fbo 1** (required for features below):
    * `screenMap` texture rendering for realistic environment reflections
    * Multisample anti-aliasing (**\r_ext_multisample**)
    * Supersample anti-aliasing (**\r_ext_supersample**)
    * Per-window gamma-correction (important for OBS/screen-capture tools)
    * High dynamic range render targets (**\r_hdr 1**), prevents color banding
    * Bloom post-processing effect
    * Arbitrary resolution rendering
    * Greyscale mode
* Triple-buffered V-Sync for low-latency and smooth frame pacing
* VRR (Adaptive Sync/G-SYNC/Freesync) support where available
* You can minimize the game window any time during **\video** or **\video-pipe** recording


## OpenGL renderer

* OpenGL 1.1 compatible (uses features from newer versions when available)
* High-quality per-pixel dynamic lighting (**\r_dlightMode**)
* Merged lightmaps (atlases)
* Static world surfaces cached in VBO (**\r_vbo 1**)
* All offscreen rendering features available as in Vulkan renderer
* Bloom reflection post-processing effect


## DirectX 12 renderer

* Modern Graphics API: Full DirectX 12 support (feature level 12.0+)
* Triple buffering for smooth frame presentation
* Command lists and descriptor heaps for optimized resource and command management
* Root signatures and pre-compiled pipeline state objects
* Resource barriers and fence-based GPU sync
* Multiple render target (MRT) and 32-bit depth buffer support
* DXGI swap chain with flip discard model
* Optional D3D12 debug layer (in debug builds)
* Automatic detection of supported Feature Levels (12.2, 12.1, 12.0, 11.1, 11.0 fallback)
* Support for Resource Binding Tiers 1, 2, and 3
* **Ray Tracing (DXR):**
  * GPU-accelerated ray tracing (where supported)
  * Realistic ray-traced lighting and effects
  * Hardware-dependent


## Metal renderer

* Modern Metal API support (macOS/iOS)
* Triple buffering for efficient presentation
* Command buffers and encoders for rendering
* Render pipeline state objects for shaders
* Depth stencil state management
* Metal 2.0+ features: argument buffers, indirect command buffers
* Metal 3.0+ ray tracing support (macOS/iOS, where available)
* Automatic feature detection and capability queries
* iOS and macOS support with a unified codebase
* Metal Shading Language (MSL) shader compilation
* Efficient Metal resource allocation


**Requirements:**

* macOS 10.13+ or iOS 11.0+
* Metal-compatible GPU
* Xcode with Metal development tools


**Platform Support:**

* Full iOS app lifecycle management (UIApplicationDelegate)
* macOS window management (NSWindow)
* Unified platform abstraction layer for both iOS and macOS
* Automatic Metal feature detection/capability queries
* Retina display support
* Variable refresh rate (ProMotion) support
* High-DPI menu and input support on all platforms

## [Build Instructions](docs/BUILD.md)

## Links

* https://idtech3.com
* https://bitbucket.org/CPMADevs/cnq3
* https://github.com/ioquake/ioq3
* https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition
* https://github.com/OpenArena/engine
* https://github.com/tomkidd/Quake3-iOS
* https://github.com/JKSunny/Quake3e

