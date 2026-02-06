# macOS Build Guide

1. **Install prerequisites**:
   - Xcode 15+ (command-line tools enabled)
   - Vulkan SDK + MoltenVK (if you plan to run the Vulkan renderer via `VK_EXT_metal_surface`)
2. **Configure**:
   ```bash
   cd /home/tim/Desktop/idtech3glints/idtech3
   cmake -B build-macos -G Xcode -DAPPLE=ON -DUSE_RENDERER_DLOPEN=ON -DUSE_VULKAN=ON
   ```
3. **Build** (release):
   ```bash
   cmake --build build-macos --config Release
   ```
4. **Run**:
   - The bundle is `build-macos/Release/idtech3.app`. Run via Finder or:
     ```bash
     open build-macos/Release/idtech3.app
     ```
5. **Package**:
   - Use `scripts/make-macos-app.sh` to copy the binary into `release/` and embed `quake3_flat.icns`.

Notes:
   - Apple builds now compile the Cocoa entry point under `src/platform/macos/`.
   - `q3ui` and the client are linked against Cocoa, Metal, MetalKit, QuartzCore, CoreGraphics, CoreFoundation, and IOKit frameworks.
