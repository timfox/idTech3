# Development Roadmap

## Current Status (next-gen-4)

### Renderer -- Complete
- [x] Vulkan 1.4 with PBR (metalness/roughness, IBL, BRDF LUT)
- [x] Volumetric fog (froxel compute, temporal reprojection, fluid sim)
- [x] Navier-Stokes fluid simulation (GPU compute)
- [x] Shadow mapping (sun CSM, spot atlas, point cubemaps)
- [x] SMAA anti-aliasing
- [x] SSAO with blur and combine
- [x] Bloom with HDR and tonemapping (ACES/Reinhard)
- [x] Glint NDF (research paper implementation)
- [x] Post-processing: panini projection, lens effects (vignette, chromatic aberration, film grain)
- [x] SSR shader, atmospheric scattering shader, vegetation wind compute shader
- [x] GoPro-style camera lens presets (7 presets)
- [x] Flashlight / projected texture system
- [x] HDR EXR skybox with IBL (equirectangular, cubemap, spherical)
- [x] OpenEXR image format support
- [x] Water flowmap (flow vectors offset texture UVs for rivers, pools, wakes)

### Physics -- Complete
- [x] Bullet Physics (35 API functions, C++ backend)
- [x] Procedural animation controller (11-state ragdoll)
- [x] IK solvers (two-bone, CCD, foot placement, aim, look-at)
- [x] DMM deformation (FEM, Voronoi fracture, thermal, 12 materials, 10 prefabs)
- [x] Cloth simulation (XPBD, wind, pinning, sleep)

### Gameplay -- Complete
- [x] AI Director (intensity, phases, spawn budgets, zones)
- [x] GOAP (A* action planning)
- [x] Horde/swarm AI (512 agents, 4-tier LOD, flocking)
- [x] Response rules (14 criteria, weighted responses)
- [x] Choreography (timeline scenes)
- [x] Facial animation (33 flex, 25 phonemes, 11 expressions)
- [x] Dismemberment + extended gibs (16 limbs, physics gibs)
- [x] Navigation mesh (Recast/Detour, crowd simulation)
- [x] Particle system (8192 pool, billboard rendering)
- [x] Background map for menus
- [x] Dynamic window title

### Audio -- Complete
- [x] OpenAL with HRTF and EFX reverb
- [x] Geometry-based acoustics
- [x] 6 audio codecs (WAV, MP3, Opus, FLAC, WebM, Ogg)
- [x] Adaptive music with intensity-driven layers

### Video -- Complete
- [x] ROQ + modern codecs (FFmpeg, dav1d, libvpx, Theora)
- [x] OpenEXR HDR image loading

### Assets -- Complete
- [x] 7 model formats (glTF, OBJ, MD5, IQM, MDR, MD3)
- [x] 6 image formats (EXR, PNG, TGA, JPG, PCX, BMP)

### Integration -- Complete
- [x] All 16 systems wired into game loop
- [x] 64 Lua-callable engine functions
- [x] ImGui inspector (optional)
- [x] Android platform support
- [x] CI for Linux, Windows, macOS, ARM

### Tooling -- Complete
- [x] Smoke test script (10 checks)
- [x] CI with smoke test step
- [x] 8 documentation files

## Remaining Work

### Short-Term (completed)
- [x] Connect BSP geometry extraction to map loading for automatic navmesh
- [x] PostFX specialization constants in vk.c gamma pipeline
- [ ] Wire SSR/atmosphere/vegetation shaders into Vulkan render passes
- [ ] glTF GPU upload (VBOs, image_t textures for rendering)

### Medium-Term
- [x] GPU occlusion culling (r_occlusionCulling, entity bbox queries, previous-frame visibility)
- [ ] Texture compression (BC7/KTX2)
- [ ] DTLS network encryption
- [ ] clang-tidy / cppcheck static analysis

### Long-Term
- [ ] RTX ray tracing
- [ ] iOS Metal backend
- [ ] WebAssembly + WebGPU
