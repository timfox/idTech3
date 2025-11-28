# CI/CD Documentation

## Overview

The id Tech 3 engine includes comprehensive CI/CD pipelines for automated building, testing, benchmarking, and release packaging across multiple platforms.

## GitHub Actions Workflows

### CI Workflow (`.github/workflows/ci.yml`)

Runs on every push and pull request to main/develop branches:

- **Linux Builds**: GCC and Clang compilers, OpenGL and Vulkan renderers
- **Windows Builds**: Visual Studio and MinGW generators
- **macOS Builds**: x86_64 and ARM64 architectures
- **Static Analysis**: clang-tidy and cppcheck
- **Code Quality Checks**: Format checking and common issue detection

### Performance Benchmarks (`.github/workflows/benchmarks.yml`)

Runs performance benchmarks:

- **When**: On pushes to main, pull requests, weekly schedule, or manual trigger
- **Metrics**: Startup time, memory usage, binary size, build time
- **Comparison**: Automatically compares with baseline
- **Reporting**: Comments on pull requests with benchmark results

### Cross-Platform Builds (`.github/workflows/cross-platform.yml`)

Comprehensive build matrix across platforms:

- **Linux**: Ubuntu 20.04, 22.04, latest (GCC/Clang)
- **Windows**: Windows 2019, 2022 (MSVC)
- **macOS**: macOS 11, 12, latest (x86_64, ARM64, Universal)

### Release Workflow (`.github/workflows/release.yml`)

Automated release packaging:

- **Trigger**: On git tags (v*) or manual dispatch
- **Builds**: All platforms simultaneously
- **Packaging**: Creates platform-specific archives
- **Release**: Automatically creates GitHub release with downloads

## Local Scripts

### Package Release (`scripts/package_release.sh`)

Creates release packages for the current platform:

```bash
./scripts/package_release.sh [version]
```

**Features:**
- Detects platform automatically
- Packages binaries and mods
- Creates platform-appropriate archives (tar.gz for Unix, zip for Windows)
- Includes README files

**Usage:**
```bash
# Auto-detect version
./scripts/package_release.sh

# Specify version
./scripts/package_release.sh 1.0.0
```

### Run Benchmarks (`scripts/run_benchmarks.sh`)

Runs performance benchmarks locally:

```bash
./scripts/run_benchmarks.sh
```

**Features:**
- Measures startup time
- Tracks memory usage
- Records binary size
- Compares with baseline
- Generates JSON reports

**Output:**
- Results saved to `benchmarks/results_YYYYMMDD_HHMMSS.json`
- Baseline comparison if available
- Console output with metrics

## Benchmark Metrics

### Startup Time
Time taken for engine to initialize and quit (seconds).

### Memory Usage
Peak memory consumption during startup (KB).

### Binary Size
Size of compiled binaries (bytes).

### Build Time
Time taken to compile the engine (seconds).

## Release Process

### Creating a Release

1. **Tag the release:**
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

2. **GitHub Actions automatically:**
   - Builds for all platforms
   - Creates release packages
   - Uploads to GitHub Releases
   - Creates release notes

### Manual Release

1. **Trigger workflow manually:**
   - Go to Actions → Release → Run workflow
   - Enter version number
   - Click "Run workflow"

2. **Or use local script:**
   ```bash
   ./scripts/package_release.sh 1.0.0
   ```

## Platform-Specific Notes

### Linux

**Dependencies:**
- SDL2, curl, OpenSSL, JPEG, Vorbis, Ogg, FreeType, SQLite, Lua, WebSockets, Zstd

**Build:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Windows

**Dependencies:**
- Visual Studio 2022 or MinGW
- vcpkg for libraries (optional)

**Build:**
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### macOS

**Dependencies:**
- Homebrew packages: cmake, sdl2, curl, openssl, jpeg, vorbis, ogg, freetype, sqlite, lua@5.4, libwebsockets, zstd

**Build:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

**Universal Binary:**
```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

## Benchmark Baseline

The first benchmark run creates a baseline (`benchmarks/baseline.json`). Subsequent runs compare against this baseline to detect performance regressions.

**Updating Baseline:**
```bash
cp benchmarks/results_YYYYMMDD_HHMMSS.json benchmarks/baseline.json
```

## CI/CD Best Practices

1. **Always test locally first:**
   ```bash
   ./scripts/run_benchmarks.sh
   ```

2. **Check build on multiple platforms:**
   - Use GitHub Actions matrix builds
   - Test on local VMs if possible

3. **Monitor benchmark trends:**
   - Review benchmark results in PR comments
   - Investigate significant changes (>10%)

4. **Version releases properly:**
   - Use semantic versioning (v1.0.0)
   - Tag releases with descriptive messages
   - Update CHANGELOG.md

5. **Keep dependencies updated:**
   - Update GitHub Actions workflows
   - Test with latest compiler versions
   - Monitor security advisories

## Troubleshooting

### Build Failures

**Linux:**
- Check installed dependencies
- Verify compiler version
- Check CMake logs

**Windows:**
- Ensure Visual Studio is installed
- Check vcpkg integration
- Verify PATH includes build tools

**macOS:**
- Update Homebrew: `brew update`
- Install missing dependencies
- Check Xcode command line tools

### Benchmark Failures

- Ensure engine binary exists
- Check file permissions
- Verify benchmark script is executable
- Review error messages in JSON output

### Release Failures

- Verify all platform builds succeed
- Check artifact uploads
- Ensure GitHub token has release permissions
- Review workflow logs

## Future Enhancements

Planned improvements:
- **Docker containers** for consistent build environments
- **Automated testing** with game scenarios
- **Performance regression detection** with alerts
- **Automated dependency updates** (Dependabot)
- **Code coverage** reporting
- **Security scanning** (CodeQL)
- **Automated changelog** generation

