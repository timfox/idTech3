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
* Froxel-based Volumetric Lighting with 2D Navier–Stokes fluid solver
* MSAA and SMAA anti-aliasing

**Audio**:
* OpenAL backend with HRTF for 3D positional audio
* Real-time reverb and occlusion effects via OpenAL EFX (heuristic environmental acoustics)
* Audio codec support: mp3, ogg, wav, flac, webm, opus

**Video**:
* Video codec support: RoQ, WebM (VP8/VP9), Ogg Theora, MP4 (H.264)

**Scripting**:
* Support for Lua scripting
* Support for JavaScript scripting

**Image Generation**:
* FLUX.2/FLUX.1 C image generation (optional, text-to-image from console with **real-time hot-reloading** and device selection; supports flux1-schnell, flux1-dev, flux2-dev; requires model files in game directory; Metal, BLAS, or pure C backend, with graceful fallback)

### Platforms

* Windows
* Linux
* Android
* MacOS

### Standards

* Engine code targets **C23**, with incremental modernization for safety and portability.
* Engine-internal code prefers native C `bool`; legacy `qboolean` is retained where required for compatibility.

### Links

* https://idtech3.com
* https://github.com/jksunny/quake3e