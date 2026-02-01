# id Tech 3

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE.md) [![Stars](https://img.shields.io/github/stars/timfox/idTech3?style=social)](https://github.com/timfox/idTech3) [![Forks](https://img.shields.io/github/forks/timfox/idTech3?style=social)](https://github.com/timfox/idTech3/forks)

This is a modernized id Tech 3 engine.

Go to [Releases](../../releases) section to download latest binaries for your platform.

*This repository does not contain any game content from Quake III Arena*

### Features

**Rendering**:
* OpenGL renderer
* Vulkan renderer
* Physically Based Rendering

**Image Generation**:
* FLUX.2 C image generation (optional, text-to-image from console with **real-time hot-reloading** and device selection; requires ~16GB model files in game directory; Metal, BLAS, or pure C backend, with graceful fallback)

**Audio**:
* OpenAL backend with HRTF for 3D positional audio
* Heuristic acoustics: real-time reverb/occlusion using OpenAL EFX

### Standards

- Engine code targets **C23**, with incremental modernization for safety and portability.
- Engine-internal code prefers native C `bool`; legacy `qboolean` is retained where required for compatibility.


### Links

* https://idtech3.com
* https://github.com/jksunny/quake3e
