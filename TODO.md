## Structured Logging
- [ ] Migrate all logging to a structured logger with levels, categories, JSON output (currently uses `Com_Printf`)
- [ ] Add log rotation and filtering
- [ ] Integrate with external logging/monitoring systems

## Memory Safety & Profiling
- [x] ASan/UBSan supported via CMake option (`ENABLE_SANITIZERS`)
- [ ] Provide documented workflows for ASan/UBSan
- [ ] Add Valgrind/Dr. Memory integration for leak and error detection
- [ ] Add memory usage tracking/stats in engine

## Performance Profiling
- [ ] Tracy Profiler integration (in progress; currently using homegrown debug timers)
- [ ] Built-in performance counters: FPS, frame times, draw calls
- [ ] GPU timing queries for renderer (partially stubbed)

## Multi-threading
- [x] Thread pool for async operations (basic version present)
- [ ] Expand to proper job system (asset loading, etc.)
- [ ] Add lock-free data structures (where applicable)

## Code Quality & Tooling
- [x] clang-format configuration in repo (see `.clang-format`)
- [ ] pre-commit hook for format enforcement
- [ ] Expand unit tests (math, memory, networking—partial coverage)
- [ ] Code coverage reporting (gcov/lcov)

## Developer Experience
- [x] Asset pipeline tools support auto-conversion (see `tools/asset_conv`)
- [ ] Hot reloading for game code (QVM)
- [ ] Improved debugging tools (ImGui debug overlays; basic overlay exists)
- [ ] Better pipeline automation and validation

## Documentation
- [x] Doxygen API docs (see `docs/`)
- [ ] Keep architecture documentation up to date
- [ ] Performance tuning guides

## Security Hardening
- [ ] Fuzzing (AFL++, libFuzzer) for network and file parsing - basic harness only
- [ ] Stack canaries and security flags (partial, check all targets)
- [x] Some input validation improvements in progress

## Modern C Features & Practices
- [x] C23 feature usage where supported (some attributes/typeof in use)
- [ ] Broader adoption (nullptr, modern generics)
- [ ] Audit for type safety (const correctness, stronger types)

## CI/CD & Automation
- [x] CI for Windows, macOS, Linux builds (see `.github/workflows/`)
- [ ] Automated performance benchmarks
- [ ] Automated release packaging (only manual currently)

## Feature Improvements & Expansions
- [ ] Better audio system (OpenAL Soft planned, still using legacy)
- [ ] Improved physics (investigate bullet3 or similar)
- [ ] Improved networking (DTLS/NAT traversal candidates under review)