# Development Roadmap

## Current Status (next-gen-2)

### Complete
- [x] Vulkan 1.4 renderer with PBR materials (metalness/roughness)
- [x] Image-based lighting (IBL) with BRDF LUT
- [x] Volumetric fog (froxel-based, temporal, compute)
- [x] Navier-Stokes fluid simulation (GPU compute)
- [x] Shadow mapping (sun CSM, spot atlas, point cubemaps)
- [x] SMAA anti-aliasing
- [x] SSAO with blur and combine
- [x] Bloom with HDR and tonemapping (ACES/Reinhard)
- [x] PBR material extensions: emissive, clearcoat, sheen, anisotropy, transmission, subsurface
- [x] Modern video codecs: FFmpeg, dav1d (AV1), libvpx (VP8/VP9), Theora
- [x] Audio codecs: WAV, MP3, Opus, FLAC, WebM
- [x] OpenAL spatial audio with HRTF and EFX reverb
- [x] Lua and Duktape scripting support
- [x] Cross-platform: Linux, Windows, macOS (x86_64, ARM)

### In Progress
- [ ] Full integration testing for modern video codecs with game data
- [ ] CI test automation beyond build validation

## Short-Term Priorities

### Renderer Quality
- [ ] Screen-space reflections (SSR)
- [ ] Temporal anti-aliasing (TAA) for volumetric fog stabilization
- [ ] Depth of field with circle-of-confusion bokeh
- [ ] Camera and per-object motion blur via velocity buffer
- [ ] HBAO+ or GTAO for improved ambient occlusion

### Asset Pipeline
- [ ] glTF 2.0 model loader for modern PBR assets
- [ ] KTX2/BC7 texture compression support
- [ ] Asset validation and conversion tools

### Quality Assurance
- [ ] Automated smoke tests in CI
- [ ] clang-tidy static analysis integration
- [ ] cppcheck integration
- [ ] Code formatting enforcement (.clang-format)

## Medium-Term Goals

### Performance
- [ ] GPU-driven rendering (indirect draw, compute culling)
- [ ] Mesh shader support for dense geometry
- [ ] Async compute for volumetric fog pipeline
- [ ] LOD improvements for vegetation and terrain

### Networking
- [ ] DTLS encryption for network traffic
- [ ] Protocol versioning and negotiation
- [ ] Rate limiting improvements

### Scripting
- [ ] Scripting API documentation
- [ ] Hot-reload for development iteration
- [ ] Script profiling and debugging tools

## Long-Term Vision

### Ray Tracing
- [ ] RTX hardware ray tracing for reflections, shadows, and GI
- [ ] Hybrid rendering (rasterization + selective ray tracing)
- [ ] Quality presets (performance/balanced/quality)
- [ ] Automatic fallback on non-RTX hardware

### Platform Expansion
- [ ] Android NDK + Vulkan support
- [ ] iOS Metal backend
- [ ] WebAssembly + WebGPU for browser deployment

### Engine Modernization
- [ ] Entity-Component-System via EnTT (headers already vendored)
- [ ] Advanced audio: geometry-based reverb, binaural rendering
- [ ] Network replay and spectator systems
