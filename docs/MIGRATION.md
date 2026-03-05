# Migration Guide

This document provides upgrade guidance when moving between engine versions.

## Versioning

The engine uses [Semantic Versioning](https://semver.org/): `MAJOR.MINOR.PATCH`.

- **MAJOR**: Breaking changes (API, file formats, network protocol)
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, backward compatible

## Upgrade Checklist

1. **Backup** your game data, configs, and mods before upgrading
2. **Read** the CHANGELOG.md for the target version
3. **Test** in a development environment before production
4. **Update** any custom scripts or mods that rely on deprecated APIs

## Common Migration Scenarios

### Renderer Changes

- **r_fbo / r_hdr**: If upgrading from an older build, run `vid_restart` after changing these cvars. Some combinations require a full restart.
- **Vulkan vs OpenGL**: Switching renderers may require `vid_restart`. Save configs before switching.

### Cvar Changes

- **Renamed cvars**: Check CHANGELOG for renames. Old names may log a warning with the new name.
- **Removed cvars**: Deprecated cvars are removed after the deprecation period (see DEPRECATION_POLICY.md).

### Mod Compatibility

- **Lua API**: New functions are additive. Removed or changed functions are documented in CHANGELOG.
- **QVM**: Game DLLs may need recompilation for ABI changes. Check release notes.

### Build System

- **CMake**: Minimum version is 3.24. Upgrade with `cmake --version`.
- **Compilers**: GCC 15+ or Clang 18+ recommended. See DEVELOPMENT_SETUP.md.

## Getting Help

- **Bugs**: Open an issue with version, platform, and steps to reproduce
- **Questions**: Check docs/ and CHANGELOG first
- **Breaking changes**: Tagged in CHANGELOG under "Changed" or "Removed"
