# Contributing to id Tech 3

Thank you for your interest in contributing. This document outlines how to get started, run tests, and submit changes.

## Before You Start

- Read [CLAUDE.md](../CLAUDE.md) for project goals, architecture, and coding conventions
- Read [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) for build prerequisites and commands
- Ensure your changes align with the layer cake (vanilla/chocolate/layercake) and never break backward compatibility

## Building

```bash
# Vulkan renderer
./scripts/compile_engine.sh vulkan

# Debug build
./scripts/compile_engine.sh vulkan debug

# Clean build
./scripts/compile_engine.sh clean vulkan
```

## Running Tests

### Smoke Test

Validates binaries and dedicated server startup:

```bash
./scripts/smoke_test.sh release
```

### CTest (from build directory)

```bash
cd build-vk-Release  # or your build dir
ctest --output-on-failure -V
```

Tests include:
- `smoke_test` - Binary checks, server startup, shader validation
- `check_artifacts` - Artifact format verification
- `unit_macros` - PAD, PADLEN, MAX_QPATH, MAX_STRING_CHARS

### Before Submitting

1. **Build** with `./scripts/compile_engine.sh vulkan`
2. **Run smoke test** - `./scripts/smoke_test.sh release`
3. **Run CTest** - `cd build-vk-Release && ctest -V`
4. **Check CI** - Push to a branch; CI runs on `main`, `next-gen*`, and related branches

## Code Quality

- Follow naming conventions in [CLAUDE.md](../CLAUDE.md)
- Add logging for new code paths
- Include error handling for edge cases
- Prefer small, reviewable commits

## Good First Issues

- Fix compiler warnings in project-owned code
- Add unit tests for `q_shared` string functions (`Q_strncpyz`, `Com_sprintf`, `Info_*`)
- Document non-obvious render flags and constants
- Update [ARM_RASPBERRY_PI.md](ARM_RASPBERRY_PI.md) with platform-specific notes

## Pull Request Process

1. Create a branch from `main` or the target branch
2. Make focused commits with clear messages
3. Ensure CI passes (see [.github/workflows/build.yml](../.github/workflows/build.yml))
4. Request review; address feedback

## Questions

- Check [ARCHITECTURE.md](ARCHITECTURE.md) and [RENDERERS.md](RENDERERS.md) for technical details
- See [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) for release validation steps
