# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Modern C23/C++23 standards implementation
- Vulkan 1.4 renderer with RTX hardware acceleration support
- Entity Component System (ECS) architecture
- Qt integration for modern UI components
- Android platform support with Gradle build system
- Asset cooking pipeline with KTX2 and RGB9E5 texture format support
- Mesh shader system with GPU-driven rendering
- Layered material system with modern math library
- OpenAL streaming and environmental audio enhancements
- OOP entity architecture with legacy compatibility layer
- Enhanced filesystem management and debug drawing functions
- Game module definitions (g_local.h)
- Performance monitoring and debugging tools
- Automated testing and benchmarking scripts
- Smart launcher with GPU auto-detection
- Font rendering with OTF, TTF, and Glyph support
- SysCall registry system
- Job system and multi-threading support
- High dynamic range (HDR) rendering
- Bloom post-processing effects
- Multisample and supersample anti-aliasing
- Screen space reflections with screenMap textures
- Physically based rendering (PBR) pipeline
- Ray tracing for volumetrics and global illumination
- Dynamic resolution and FSR support
- Steamworks integration and Steam Deck compatibility

### Fixed
- Type casting issues in botlib and memory management
- Buffer size problems in common libraries
- Unused variable warnings in ECS and file systems
- Android Gradle configuration compatibility
- Theora debug logging replacement (Com_Printf → Com_DPrintf)

### Known Issues
- Vulkan renderer requires compatible hardware (automatic fallback to OpenGL)
- RTX features require NVIDIA RTX GPUs
- Some legacy mods may need compatibility updates
- Android build requires specific Gradle/SDK versions

## [0.1.0-preview] - 2026-01-18

### Added
- Initial release with modernized id Tech 3 engine
- Vulkan renderer implementation
- OpenGL fallback renderer
- Basic ECS integration
- Android platform support
- Enhanced error handling and memory safety
- Performance monitoring tools
- Automated build and testing scripts

### Fixed
- Core memory safety issues
- Type casting and buffer overflow problems
- Basic compatibility with legacy Quake III Arena assets

### Known Issues
- Limited ECS component coverage
- Android build configuration may need platform-specific tuning
- Some advanced Vulkan features may require driver updates
- Documentation incomplete for new features

---

## Release Process

### Version Numbering
- **Major version (X.y.z)**: Breaking changes, major feature additions
- **Minor version (x.Y.z)**: New features, enhancements
- **Patch version (x.y.Z)**: Bug fixes, optimizations
- **Preview suffix (-preview)**: Pre-release versions for testing

### Release Checklist
- [ ] Update CHANGELOG.md with all changes since last release
- [ ] Update version numbers in relevant files
- [ ] Run full test suite (`./scripts/test_engine.sh`)
- [ ] Build and test on all supported platforms
- [ ] Create git tag with version number
- [ ] Push tag to trigger automated releases
- [ ] Update documentation if needed

### Categories for Changes
- **Added**: New features, capabilities, or assets
- **Changed**: Modifications to existing functionality
- **Deprecated**: Features marked for removal in future versions
- **Removed**: Deleted features or breaking changes
- **Fixed**: Bug fixes and corrections
- **Security**: Security-related fixes or improvements
- **Performance**: Performance improvements or optimizations

---

*For the latest updates and detailed commit history, see the [GitHub repository](https://github.com/your-repo/idtech3).*"