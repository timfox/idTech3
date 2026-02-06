# iOS Build Stub Guide

1. **Prerequisites**:
   - Xcode 15+ with iOS SDKs.
   - CMake with Apple generators (`-G Xcode`).
2. **Configure for iOS** (e.g., simulator):
   ```bash
   cmake -B build-ios -G Xcode -DAPPLE=ON -DCMAKE_SYSTEM_NAME=iOS -DTARGET_OS_IPHONE=1
   ```
3. **Build**:
   ```bash
   cmake --build build-ios --config Release
   ```
4. **Run**:
   - Open `build-ios/idtech3.xcodeproj` in Xcode, choose a simulator/device, and run.

The current iOS target is a stub: it compiles `src/platform/ios/*` and implements basic UIApplication lifecycle hooks but does not yet connect a renderer. This gives you a starting point for future Metal/Vulkan work on iOS.
