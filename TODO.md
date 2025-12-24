## AAA-Grade Observability & Telemetry

### Advanced Structured Logging & Monitoring
- [x] **Enterprise Logging Pipeline**: Structured JSON logging with levels, categories, correlation IDs (see `src/common/q_log.h/c`, `Com_Printf` routes through structured logger via `Q_Log_ComPrintf()`)
- [x] **Distributed Tracing**: Request correlation across engine subsystems with OpenTelemetry integration
- [x] **Performance Telemetry**: Real-time performance metrics streaming to monitoring systems
- [x] **Log Rotation & Filtering**: Advanced log management with compression and retention policies (see `log_rotation_size`, `log_rotation_time`, `log_category_filter` CVars, implemented in `q_log.c`)
- [x] **ELK/Loki Integration**: Full integration with modern observability stacks (syslog, structured JSON, metric export)
- [ ] **Crash Telemetry**: Automatic crash reporting with minidump + context data upload
- [ ] **Player Analytics**: Privacy-compliant gameplay metrics and performance data collection

## Enterprise Memory Management

### Advanced Memory Safety & Performance
- [x] **Sanitizer Integration**: ASan/UBSan/TSan support with automated CI workflows (`ENABLE_SANITIZERS`)
- [x] **Memory Safety Workflows**: Comprehensive documentation and CI validation (see `docs/MEMORY_SAFETY_WORKFLOWS.md`)
- [x] **Leak Detection Pipeline**: Valgrind/Dr.Memory integration with automated reporting (see `tools/run_valgrind.sh`, `tools/run_drmemory.bat`)
- [ ] **GPU Memory Tracking**: VRAM usage monitoring and leak detection for Vulkan resources
- [ ] **Memory Defragmentation**: Runtime memory pool defragmentation for long-running sessions
- [ ] **Predictive Allocation**: ML-driven memory allocation based on usage patterns
- [ ] **Memory Pool System**: Hierarchical memory pools with automatic scaling and cleanup
- [x] **Memory Analytics**: Real-time memory usage tracking and statistics (see `src/common/memory_stats.h/c`, console command `memstats`)

### Advanced Memory Features
- [ ] **Lock-free Allocators**: High-performance concurrent memory allocation
- [ ] **Arena Allocators**: Scoped memory management for subsystems (rendering, audio, networking)
- [ ] **Memory Advisor**: Intelligent memory layout optimization based on access patterns
- [ ] **Cache-Conscious Data Structures**: Optimized for modern CPU cache hierarchies

## AAA Performance Profiling

### Advanced Profiling & Optimization
- [x] **Tracy Integration**: Real-time profiler with GPU zones and memory tracking (see `CMakeLists.txt` USE_TRACY option, `src/common/profiler.h`)
- [x] **GPU-Driven Profiling**: Hardware performance counters and pipeline statistics (see `src/renderers/vulkan/vk.c` timing queries)
- [ ] **Render Graph Profiler**: Detailed per-pass performance analysis with bottleneck identification
- [ ] **Memory Bandwidth Profiler**: Cache miss analysis and memory access pattern optimization
- [ ] **Parallel Processing Profiler**: Thread utilization and synchronization overhead tracking
- [ ] **Shader Performance Analysis**: Instruction count, register usage, and optimization suggestions
- [ ] **Asset Loading Profiler**: Streaming performance and I/O bottleneck identification

### Real-time Performance Monitoring
- [x] **Built-in Counters**: FPS, frame times, draw calls, GPU memory usage (see `src/common/performance_counters.h/c`)
- [ ] **Performance HUD**: Real-time overlay with bottleneck highlighting and recommendations
- [ ] **Automated Performance Regression Detection**: CI-based performance gates with historical comparison
- [ ] **Heatmap Visualization**: Performance data visualization for optimization focus areas
- [ ] **Performance Presets**: Quality vs performance trade-off configurations

## Modern Multi-threading Architecture

### Advanced Job System & Concurrency
- [x] **Job System Foundation**: Fiber-based job scheduling with work-stealing queues
- [x] **Async Asset Pipeline**: Parallel loading for textures, models, sounds (FS_ReadFile_Async, R_LoadImage_Async)
- [ ] **GPU-Async Compute**: GPU-driven job scheduling with CPU-GPU synchronization
- [ ] **Task Dependencies**: Advanced task graph with automatic dependency resolution
- [ ] **Load Balancing**: Dynamic thread pool scaling based on workload characteristics
- [ ] **Worker Affinity**: CPU core pinning for specialized workloads (rendering, audio, physics)

### Lock-Free Data Structures & Synchronization
- [ ] **Lock-Free Queues**: High-performance concurrent queues for inter-thread communication
- [ ] **Atomic Operations**: Extensive use of C11/C23 atomics for thread-safe operations
- [ ] **Hazard Pointers**: Memory-safe concurrent data structures
- [ ] **Read-Copy-Update (RCU)**: Efficient concurrent read operations
- [ ] **Spin Locks**: Low-latency synchronization for high-frequency operations

### Threading Subsystems
- [ ] **Rendering Threads**: Dedicated threads for different rendering phases
- [ ] **Audio Thread**: Isolated audio processing with minimal latency
- [ ] **Network Threads**: Dedicated networking with lock-free message queues
- [ ] **Streaming Thread**: Background asset streaming with priority management

## Enterprise Code Quality & DevOps

### Modern Development Workflow
- [x] **Code Formatting**: clang-format with pre-commit hooks (see `.clang-format`, `.git/hooks/pre-commit`)
- [x] **Static Analysis**: Comprehensive clang-tidy, cppcheck, and custom analyzers (see `.clang-tidy`, `cppcheck.cfg`, `tools/run_clang_tidy.sh`, `tools/run_cppcheck.sh`)
- [x] **Zero-Warning Policy**: Clean compilation across all platforms and configurations
- [ ] **Automated Code Review**: AI-assisted code review with style and best practice checks
- [ ] **Live Code Analysis**: Real-time feedback during development with IDE integration

### Advanced Testing Infrastructure
- [x] **Unit Test Suite**: Comprehensive test coverage for math, memory, networking (see `tests/` directory)
- [x] **Property-Based Testing**: 3300+ test cases with fuzzing integration (see `tests/test_property_based_simple.c`)
- [x] **Security Testing**: Input validation and attack vector testing (see `tests/test_security.c`)
- [ ] **Performance Regression Tests**: Automated performance validation in CI
- [ ] **Cross-Platform Compatibility Tests**: Automated testing across all supported platforms
- [ ] **Memory Safety Tests**: ASan/UBSan validation for all code paths
- [ ] **Thread Safety Tests**: Race condition detection and validation

### Code Coverage & Quality Metrics
- [x] **Coverage Reporting**: gcov/lcov with HTML reports and CI integration (see `CMakeLists.txt` ENABLE_COVERAGE option, `tools/run_coverage.sh`)
- [ ] **Code Quality Gates**: Minimum coverage requirements and complexity limits
- [ ] **Technical Debt Tracking**: Automated monitoring of code quality metrics
- [ ] **Performance Benchmarking**: Automated performance regression detection

### Modern Language Features & Best Practices
- [x] **C23 Adoption**: Modern C features with backwards compatibility
- [ ] **Type Safety**: Comprehensive const correctness and stronger typing
- [ ] **Error Handling**: Structured error handling with stack traces
- [ ] **Resource Management**: RAII patterns and automatic resource cleanup
- [x] Font rendering fixes for all renderers (fixed extern function declarations in text rendering, added R_InitFonts to Metal renderer, integrated FreeType library for proper font loading, resolved CVar linking conflicts between renderers, set up font assets in mymod, enhanced UTF-8 Unicode support, improved kerning and glyph fallback handling, renderer-level font redirection for compatibility)
- [x] Vulkan renderer improvements (enabled experimental features, added bindless texture system, shader caching, async compilation framework, dynamic rendering support, GPU culling, mesh shaders, ray tracing, fixed font CVar linking issues - see `src/renderers/vulkan/`)
- [x] Renderer selection fixes (changed default renderer to Vulkan, fixed OpenGL interface initialization by adding USE_OPENGL_API to renderer targets, improved renderer fallback logic, resolved DLL symbol issues for font CVars)

## Developer Experience
- [x] Asset pipeline tools support auto-conversion (see `tools/asset_conv`)
- [x] Hot reloading for game code (QVM) (see `src/common/vm_hot_reload.h/c`, `docs/VM_HOT_RELOAD.md`, `tools/build_and_reload_qvm.sh`)
- [x] Improved debugging tools (ImGui debug overlays: Performance, Memory, Network, Renderer, CVar Browser, Console, Event System, Profiler - see `src/client/cl_imgui_debug.c`, `docs/imgui-debug-overlays.md`)
- [x] Better pipeline automation and validation (see `tools/validate_assets.py`, `docs/ASSET_VALIDATION.md`, CI integration in `.github/workflows/ci.yml`)

## Documentation
- [x] Doxygen API docs (see `docs/`)
- [ ] Keep architecture documentation up to date
- [x] Performance tuning guides (see `docs/PERFORMANCE_TUNING_GUIDE.md` - comprehensive guide covering CPU/GPU/memory/network optimization, profiling tools, CVar reference, benchmarking, troubleshooting)

## Security Hardening
- [x] Fuzzing (AFL++, libFuzzer) for network and file parsing - basic harness only - Expanded with BSP, shader script, and PK3 parsing fuzzers
- [ ] Stack canaries and security flags (partial, check all targets)
- [x] Some input validation improvements in progress - Implemented filename validation, integer string validation, and command injection prevention in tests/test_security.c

## Modern C Features & Practices
- [x] C23 feature usage where supported (some attributes/typeof in use) - Documented in docs/MODERN_CPP_FEATURES.md
- [x] Broader adoption (nullptr, designated initializers, static_assert) (see `docs/MODERN_CPP_FEATURES.md`)
- [x] Audit for type safety (const correctness, stronger types) (see `docs/TYPE_SAFETY_AUDIT.md`) - Comprehensive type safety improvements documented

## Enterprise CI/CD & DevOps

### Advanced Build Pipeline
- [x] **Multi-Platform CI**: Windows, macOS, Linux with cross-compilation support
- [ ] **Incremental Builds**: Change detection and selective recompilation
- [ ] **Distributed Builds**: Build farm integration for faster compilation
- [ ] **Build Optimization**: Link-time optimization and binary size reduction
- [ ] **Asset Cooking Pipeline**: Automated asset processing and optimization

### Automated Testing & Validation
- [ ] **Performance Benchmarks**: Automated performance regression testing
- [ ] **Compatibility Testing**: Cross-platform and cross-hardware validation
- [ ] **Asset Validation**: Automated asset correctness and optimization checking
- [ ] **Binary Analysis**: Automated security scanning and optimization analysis

### Release Engineering
- [ ] **Automated Packaging**: Multi-platform installer generation
- [ ] **Update System**: Incremental patch generation and distribution
- [ ] **Content Delivery**: CDN integration for asset streaming

### DevOps Integration
- [ ] **Infrastructure as Code**: Automated environment provisioning
- [ ] **Monitoring Integration**: Build metrics and performance tracking
- [ ] **Security Scanning**: Automated vulnerability detection and patching

## Feature Improvements & Expansions
- [ ] Better audio system (OpenAL)
- [ ] Improved physics (bullet3)
- [ ] Improved networking (DTLS/NAT traversal candidates under review)

## OOP / Entity Architecture
- [x] Decide OOP strategy for game VM: use C++ for new gamecode, keep a stable C ABI shim for engine/VM boundaries and QVM compatibility
- [x] Document C/C++ boundary rules (extern "C", POD layouts for net/save structs, no RTTI/exceptions across boundary - see `docs/C_CPP_BOUNDARY_RULES.md`)
- [ ] Design `BaseEntity` interface: Spawn/Precache, Think/ScheduleNextThink, Touch/Use, TakeDamage/Pain/Death, Save/Restore hooks
- [ ] Add classname→factory registry so entities spawn via class descriptors (HL-style), with backwards-compatible fallbacks to current spawn funcs
- [ ] Implement per-entity vtable/method table and shared mixins for movement/physics/rendering so code reuse mirrors HL/HL2 (e.g., door/trigger/npc behaviors)
- [ ] Provide message dispatch helpers (FireOutput, Input handlers) and a simple event bus for entities to communicate without hard coupling
- [ ] Define network/save descriptors per class (fields, serializers) to unify snapshot + save/load logic; include versioning and defaulting
- [ ] Port pilot entities to the new model (e.g., `func_door`, `trigger_multiple`, one NPC) and benchmark parity with legacy paths

## Steam Deck Preparation
- [ ] Default controller configuration enables access to all in-game functionality (Steam Input or native gamepad support strongly recommended)
- [ ] Add or supply a controller configuration mapping all required inputs if native support is missing
- [ ] Ensure on-screen keyboard automatically appears for all text input fields (use Steamworks: ShowFloatingGamepadTextInput or ShowGamepadTextInput where possible)
- [ ] Support simultaneous mouse-style (gyro/trackpad/1:1) and joystick-style camera input—avoid input lockout or incorrect prompt switching
- [ ] Prioritize Vulkan as primary graphics API (Unity/Unreal: enable Vulkan for all users; otherwise, use Proton's DX-to-Vulkan fallback)
- [ ] Prefer standalone video/audio codecs (VP9/AV1) over vendor-locked ones (WMF, etc)
- [ ] Implement save game cloud sync via Steam Cloud (or 3rd-party service); avoid syncing device-specific settings (e.g., resolution)
- [ ] Ensure all singleplayer/offline-capable content is playable fully offline—including first run and new user setup
- [ ] Eliminate or minimize use of custom game launchers; if unavoidable, support controller input translation (see SetGameLauncherMode in Steamworks SDK)

- [ ] Draft initial `script_api.h/cpp` skeleton for engine-side Lua runtime
- [ ] Embed Lua VM; bind minimal API: `print`, `wait`, `Game.Log`, `Game.SetObjective`
- [ ] Implement coroutine-powered script runner and `start_script` infra
- [ ] Entity script class: hook up `OnSpawn`, `OnTakeDamage`, `OnUse`, `OnDeath`
- [ ] Implement `wait(seconds)` and `wait_for_event(event, filterFn)`—coroutine scheduler
- [ ] Event bus: engine emits, Lua `Events.on(event, fn)` can subscribe
- [ ] Level scripts: convention for `level_init` entrypoint (`scripts/levels/<map>.lua`)
- [ ] Designer DSL layer: implement `Game`, `Events`, `Sequence`, `Encounter` in pure Lua
- [ ] Encounter state machine helper (`Encounter.define`, state transitions, triggers)
- [ ] Sequence/cinematic builder (`Sequence.define`, step/timeline API)
- [ ] Sample scripts: enemy behavior, encounter definition, mission sequence
- [ ] Hot reload for scripts (reload Lua states on file save)
- [ ] In-game script inspector (list running coroutines, encounters, sequences, entity script states)
- [ ] Script-side logging with source info for debugging
- [ ] Unit test harness for core Lua helpers (wait, event, encounter, sequence)
- [ ] Docs: quickstart for designers (core Lua API, hooks, patterns, examples)
- [ ] Long-term: Visual scripting or graph builder (optional, phase 2+)
- [ ] QOL: console command to reload scripts, restart sequences, dump active scripts

---

## Build & Platform Hardening

- [x] **Reproducible builds** (pinned toolchains, deterministic archives, `SOURCE_DATE_EPOCH`)
- [ ] **Package manager integration** (vcpkg/Conan presets; lockfile + CI cache)
- [ ] **Compiler matrix** (GCC/Clang/MSVC, `-Werror` on CI, warning budgets)
- [ ] **LTO/PGO toggles** (CMake options + docs + CI artifact)
- [ ] **Cross compilation** (Steam Deck / Linux cross toolchain; `x86_64` + `aarch64` plans)
- [ ] **Symbol + crash dump pipeline** (PDB/dSYM/DWARF upload, symbol server layout)

## Runtime Observability (beyond logging)

- [x] **Central performance monitoring** (comprehensive perf cvars: `perf_*` series)
- [x] **GPU profiling integration** (Tracy + built-in GPU timing queries)
- [x] **Frame capture hooks** (Vulkan debug markers with `r_vulkan_debug`)
- [x] **Configurable debug channels** (`r_debug_*`, `net_debug_*`, `fs_debug_*`, `perf_*` categories)
- [x] **Performance regression detection** (`perf_regression_*` cvars)
- [x] **Memory tracking system** (VRAM + system memory monitoring)

## Crash Resilience & Diagnostics

- [x] **Crash handler** (minidump generation with `com_crash_minidump` cvar)
- [x] **Log ring buffer** (configurable with `com_crash_log_ringbuffer` cvar)
- [x] **Assert strategy** (configurable levels with `com_assertLevel` cvar)
- [ ] **"Safe mode" boot** (disable renderer mods, reset config, start windowed)
- [x] **Watchdog for deadlocks** (enabled with `com_watchdogEnabled` cvar)
- [x] **Crash telemetry** (optional with `com_crash_telemetry` cvar)

## Determinism & Replayability (huge for networking + debugging)

- [ ] **Deterministic time step mode** (fixed tick, decoupled render)
- [ ] **Input recording + replay** (for bug repro and perf comparisons)
- [ ] **Deterministic RNG** (seed control per subsystem; snapshot seed state)
- [ ] **Golden test replays** in CI (run headless, validate checksums)

## Filesystem / Asset System Modernization

- [x] **Background streaming** (configurable with `fs_streaming*` cvars)
- [x] **Asset validation** (enabled with `fs_asset_validation` cvar)
- [x] **Decompression threading** (`fs_decompression_threads` cvar)
- [ ] **Virtual FS v2**: mount table, priority, write dir policy, sandboxing
- [ ] **Asset manifest + hashing** (content-addressable IDs, integrity checks)
- [ ] **Shader pipeline**: shader cache versioning, hot reload, fallback shaders
- [ ] **Texture pipeline**: KTX2/BasisU path (even if optional), mip policy, SRGB rules
- [ ] **Validation command**: `--validate-assets` (missing refs, bad paths, invalid metadata)

## Cloud-Native Networking & Multiplayer (Modern AAA Standard)

### Advanced Networking Architecture
- [x] **Protocol Versioning**: Backwards compatibility with feature negotiation (see `docs/BACKWARDS_COMPATIBILITY.md`)
- [ ] **Deterministic Networking**: Lock-step simulation for competitive gameplay
- [ ] **Client-Side Prediction**: Advanced prediction with reconciliation
- [ ] **Server Authoritative Movement**: Anti-cheat movement validation
- [ ] **Distributed Server Architecture**: Multi-server coordination for large worlds

### Quality of Service & Performance
- [ ] **Adaptive Network Quality**: Dynamic compression based on connection quality
- [ ] **Network Threading**: Dedicated network threads with lock-free queues
- [ ] **Bandwidth Optimization**: Priority-based data streaming and compression
- [ ] **Latency Compensation**: Advanced lag compensation with server rewind

### Security & Anti-Cheat
- [ ] **Cryptographic Authentication**: Secure player authentication and session management
- [ ] **Anti-Cheat Framework**: Server-side validation with client integrity checking
- [ ] **Rate Limiting**: Per-IP and per-client rate limiting with abuse detection
- [ ] **DDoS Protection**: Distributed denial of service mitigation

### Cloud Integration
- [ ] **Matchmaking Service**: Cloud-based matchmaking with skill rating
- [ ] **Dedicated Server Management**: Automated server provisioning and scaling
- [ ] **Player Analytics**: Privacy-compliant gameplay metrics and telemetry
- [ ] **Cross-Platform Services**: Unified services across all platforms

### NAT Traversal & Connectivity
- [ ] **STUN/TURN Server**: NAT traversal for peer-to-peer connectivity
- [ ] **Relay Network**: Fallback relay servers for difficult network conditions
- [ ] **Connection Quality Monitoring**: Real-time network quality assessment
- [ ] **Adaptive Transport**: Protocol switching based on network conditions

## Save/Load & Persistence

- [x] **Unified serialization framework** (engine + gamecode, versioned, schema-driven - see `docs/BACKWARDS_COMPATIBILITY.md` for framework design)
- [x] **Save corruption recovery** (atomic writes, backups, checksums - documented in `docs/BACKWARDS_COMPATIBILITY.md`)
- [x] **Migration tests** (load old saves in CI, auto-upgrade, verify invariants - see `tools/test_compatibility.sh`, `tools/migrate_save.py`, `docs/BACKWARDS_COMPATIBILITY.md`)

## AAA Rendering Pipeline

### Advanced GPU Architecture
- [x] **GPU-Driven Rendering**: Compute shader based culling and LOD selection
- [x] **Render Graph System**: Explicit pass dependencies and resource barriers
- [x] **Pipeline State Objects**: Pre-compiled pipeline states for performance
- [ ] **GPU Memory Management**: Advanced VRAM allocation and defragmentation
- [ ] **Shader Permutation System**: Dynamic shader variant generation and caching

### Next-Gen Rendering Features
- [x] **Vulkan Validation**: Comprehensive GPU debugging and validation workflows
- [x] **Pipeline Cache**: Persistent shader cache with invalidation rules
- [ ] **Mesh Shaders**: Modern mesh pipeline with task/amplification shaders
- [ ] **Ray Tracing Pipeline**: Hardware-accelerated RT with BVH optimization
- [ ] **Variable Rate Shading**: Quality/performance scaling based on content

### Advanced Material System
- [ ] **Layered Materials**: Multi-layer material system with blending modes
- [ ] **Procedural Materials**: Runtime material generation and modification
- [ ] **Material Instances**: Efficient material variation system
- [ ] **Shader Graphs**: Visual shader authoring with node-based editing
- [ ] **Material Validation**: Real-time material correctness checking

### Post-Processing & Effects
- [x] **Tonemapping Pipeline**: Multiple tonemapping operators (ACES, Reinhard, etc.)
- [x] **Bloom System**: Advanced bloom with lens flare and glare effects
- [ ] **Depth of Field**: Bokeh depth of field with custom aperture shapes
- [ ] **Motion Blur**: Multiple motion blur techniques (camera, object-based)
- [ ] **Chromatic Aberration**: Lens-based color fringing effects
- [ ] **Film Grain**: Temporal film grain for cinematic quality

### Advanced Lighting & GI
- [ ] **Light Probes**: Dynamic global illumination with probe lighting
- [ ] **Volumetric Lighting**: Participating media and god ray effects
- [ ] **Screen Space Effects**: SSGI, SSAO, SSR with temporal accumulation
- [ ] **Light Baking Pipeline**: Automated lightmap generation with UV optimization

### Texture & Asset Pipeline
- [ ] **Virtual Texture System**: Massive texture streaming with mip biasing
- [ ] **Texture Compression**: Multiple compression formats with quality selection
- [ ] **Asset Streaming**: Background loading with priority and prediction
- [ ] **LOD System**: Automatic level of detail with morphing transitions

### HDR & Color Pipeline
- [ ] **HDR Rendering**: Full HDR pipeline from capture to display
- [ ] **Color Management**: ACES color space with custom LUTs
- [ ] **Auto Exposure**: Dynamic exposure adjustment with eye adaptation
- [ ] **Color Grading**: Professional color correction tools

### Performance & Quality Scaling
- [x] **FSR Integration**: AMD FSR with quality/performance modes (`r_fsr*` cvars)
- [x] **GPU Validation**: Vulkan validation workflows (`r_vulkan_validation`)
- [x] **Texture Streaming**: VRAM management with eviction (`r_vram_budget`)
- [x] **Render Graph**: Basic pass system with explicit barriers (`r_render_graph`)
- [ ] **Dynamic Resolution**: Runtime resolution scaling for performance
- [ ] **Quality Presets**: Multiple quality levels with automatic detection
- [ ] **GPU Feature Detection**: Automatic feature enablement based on hardware

## Immersive Audio Engine

### Advanced Audio Architecture
- [ ] **Audio Graph System**: Node-based audio processing with modular effects
- [ ] **Spatial Audio**: HRTF and object-based audio with occlusion
- [ ] **Dynamic Mixing**: Real-time mix adjustment based on gameplay context
- [ ] **Middleware Integration**: Wwise/FMOD integration with asset pipeline

### Performance & Quality
- [ ] **Audio Profiler**: Real-time audio performance monitoring and optimization
- [ ] **Streaming Audio**: Background audio loading with priority management
- [ ] **Audio Virtualization**: CPU-efficient audio simulation for distant sources
- [ ] **Platform Optimization**: Platform-specific audio optimizations and fallbacks

## AI & Machine Learning Integration

### ML-Powered Engine Features
- [ ] **Procedural Content Generation**: ML-assisted level design and asset creation
- [ ] **Adaptive Difficulty**: Player skill assessment and dynamic difficulty adjustment
- [ ] **Performance Prediction**: ML-based performance optimization recommendations
- [ ] **Content Personalization**: Player preference learning and content adaptation

### Computer Vision & Analysis
- [ ] **Image Analysis**: Texture analysis for automatic LOD generation
- [ ] **Audio Analysis**: Automatic audio mixing and dynamic range compression
- [ ] **Performance Pattern Recognition**: Automated bottleneck identification
- [ ] **Player Behavior Analysis**: Gameplay pattern recognition for balancing

## Modern Development Ecosystem

### Live Development Tools
- [x] **Hot Reloading**: Runtime asset and code reloading (see `src/common/vm_hot_reload.h/c`)
- [ ] **Live Editing**: Real-time property editing with immediate feedback
- [ ] **Play-in-Editor**: Integrated gameplay testing within the editor
- [ ] **Collaborative Editing**: Multi-user editing with conflict resolution

### Asset Pipeline Modernization
- [ ] **Unified Asset Database**: Centralized asset management with metadata
- [ ] **Automated Processing**: ML-assisted asset optimization and compression
- [ ] **Version Control Integration**: Asset diffing and conflict resolution
- [ ] **Remote Build Farm**: Distributed asset processing and compilation

## Tooling & Content Authoring (Radiant-focused)

- [ ] **Gamepack schema + validator** (JSON schema, versioning, env var expansion tests)
- [ ] **Entity def modernization** (typed fields, defaults, ranges, UI hints)
- [ ] **Prefab system improvements** (dependency tracking, versioned prefabs, “bake to brushes”)
- [ ] **Map compile orchestration** (profiles, incremental compile, artifact caching)
- [ ] **Remote compile farm option** (later): compiler RPC protocol stub

## Testing & QA Engineering

- [x] **Performance regression detection** (automated with `perf_regression_*` cvars)
- [x] **Property tests** for math/geom (AABB, planes, winding, BSP ops) - Added vector dot/cross products, plane operations, AABB operations, sphere operations to test_property_based_simple.c; Fixed test framework integration and floating-point precision issues
- [x] **Fuzz targets expanded** (bsp, shader scripts, pk3 parsing, network messages) - Added fuzz_bsp.c, fuzz_shader.c, fuzz_pk3.c with libFuzzer integration
- [x] **Performance regression gates** (threshold-based; fail CI if >X% regression) - Implemented in .github/workflows/benchmarks.yml with automated baseline comparison
- [ ] **Headless test runner** (unit + integration + replay tests)
- [x] **Automated testing workflows** (CI validation, sanitizers, coverage)

## Security & Supply Chain

- [ ] **Dependency audit pipeline** (SBOM generation, `cargo/vcpkg` equivalent where applicable)
- [ ] **Signed releases** (sign binaries + manifests)
- [ ] **Secure defaults** (no remote downloads, no unsafe cvars in release builds)

## Modern UI/UX & Modding Ecosystem

### Advanced UI Framework
- [x] **UI2 System**: Modern C++ UI framework with layout system (see `src/ui/ui2_*`)
- [ ] **Visual UI Editor**: Drag-and-drop UI creation with live preview
- [ ] **Responsive Layouts**: Automatic layout adaptation for different screen sizes
- [ ] **Animation System**: Keyframe-based UI animations and transitions
- [ ] **Theming System**: Dynamic UI theming with CSS-like styling
- [ ] **Accessibility**: Screen reader support and keyboard navigation
- [ ] **Localization**: Real-time language switching and RTL support

### User Experience Enhancement
- [ ] **First-Run Experience**: Automated settings detection and optimization
- [ ] **Controller Support**: Advanced gamepad integration with custom bindings
- [ ] **Accessibility Options**: Colorblind modes, font scaling, audio descriptions
- [ ] **Performance Presets**: Automatic quality adjustment based on hardware
- [ ] **User Analytics**: Privacy-compliant usage metrics for UX improvement

### Modding & Community Features
- [ ] **Mod Marketplace**: In-engine mod browser with ratings and reviews
- [ ] **Workshop Integration**: Steam Workshop or equivalent mod hosting
- [ ] **Mod Dependencies**: Automatic dependency resolution and installation
- [ ] **Save Compatibility**: Mod-aware save system with version management
- [ ] **Mod Tools**: Integrated mod creation tools and documentation

### Packaging & Distribution
- [ ] **Cross-Platform Packaging**: Unified installer system for all platforms
- [ ] **Delta Updates**: Efficient patch distribution with binary diffing
- [ ] **Content Streaming**: On-demand asset downloading and caching
- [ ] **DRM-Free Options**: Flexible licensing and distribution options

## Scripting with Lua

- [ ] **Stable C API boundary for Lua bindings** (no direct engine internals)
- [ ] **Deterministic script execution rules** (what runs in tick vs frame)
- [ ] **Sandboxing** (no `os.execute`, file IO gating, whitelisted libs)
- [ ] **Error containment** (script errors don’t kill engine; quarantine failing coroutine)
- [ ] **Save/load for scripts** (serialize coroutine/encounter state or define reset policy)

### Shippability Meta

- [ ] Add **priorities + phases** to every section (P0 ship blocker / P1 / P2 nice-to-have)
- [ ] Add a **Definition of Done template** per feature (tests, docs, CI, perf impact, debug toggles)

### AI-Assisted Development
- [ ] **AI Code Generation**: ML-assisted code completion and optimization suggestions
- [ ] **Automated Testing**: AI-generated test cases and coverage analysis
- [ ] **Performance Optimization**: ML-driven performance bottleneck identification
- [ ] **Asset Generation**: Procedural content creation with AI assistance

### Metaverse & Social Features
- [ ] **Social Hub**: Integrated social features with friends, clans, and communities
- [ ] **Cross-Game Integration**: Shared progression and cosmetics across titles
- [ ] **Live Events**: Dynamic event system with real-time content updates
- [ ] **User-Generated Content**: Advanced UGC creation and moderation tools

### Advanced Physics & Simulation
- [ ] **Realistic Physics**: Advanced rigid body dynamics and soft body simulation
- [ ] **Cloth Simulation**: GPU-accelerated cloth and fabric simulation
- [ ] **Particle Systems**: Advanced particle effects with fluid dynamics
- [ ] **Destruction System**: Dynamic object destruction and debris simulation

### Neural Rendering & DLSS 4.0+
- [ ] **DLSS 3.0+ Integration**: Frame generation and super-resolution
- [ ] **Neural Rendering**: AI-assisted rendering quality improvements
- [ ] **Real-time Path Tracing**: Neural-accelerated ray tracing
- [ ] **Quality Upscaling**: Multiple AI upscaling technologies

### Cloud Gaming & Streaming
- [ ] **Cloud Rendering**: Server-side rendering with client streaming
- [ ] **Adaptive Quality**: Dynamic quality adjustment based on network conditions
- [ ] **Cross-Device Play**: Seamless gameplay across multiple devices
- [ ] **Remote Play**: Low-latency remote gameplay with cloud assistance

### Sustainability & Efficiency
- [ ] **Power Management**: Dynamic performance scaling for battery life
- [ ] **Carbon-Aware Computing**: Environmentally conscious resource usage
- [ ] **Efficient Algorithms**: Research-backed optimization techniques
- [ ] **Long-Term Support**: Extended platform support and backwards compatibility

## Event-Driven Architecture (EDA)

### Core Event System

- [x] **Define central event bus API (engine-level)** (see `src/common/event_system.h/c`, `docs/EVENT_SYSTEM.md`)
    - `publish(event)` - implemented as `Event_Publish()`
    - `subscribe(event_type, handler, priority)` - implemented as `Event_Subscribe()`
    - `unsubscribe(handle)` - implemented as `Event_Unsubscribe()`
    - Support **typed events** (struct-based, not string-only) - implemented with `event_t` struct
    - Event **categories/namespaces**: `Engine.*`, `Game.*`, `Entity.*`, `Net.*`, `UI.*` - implemented as `eventCategory_t` enum
    - **Event priority ordering**: pre, normal, post - implemented as `eventPriority_t` enum
    - **Event cancellation / consumption** semantics - basic support via event flags
    - **Event bubbling rules** (entity → world → engine, if applicable) - framework ready for expansion

### Event Lifecycle & Timing

- [x] **Explicit event phases** (see `src/common/event_system.h/c`, `docs/EVENT_PHASES.md`)
    - immediate (same tick) - implemented via `Event_PublishImmediate()`
    - deferred (end of frame) - implemented via `Event_PublishDeferred()`
    - scheduled (future tick / time) - implemented via `Event_PublishScheduled(delayMs)`
- [ ] **Deterministic event ordering guarantees** (important for net/replay)
- [ ] **Event queue flushing rules per subsystem** (render, physics, script, net)
- [ ] **Frame/tick boundary documentation** (what must not fire mid-tick)

### Threading & Concurrency

- [ ] **Thread-safe event publishing** (lock-free queue or double-buffered queues)
- [ ] **Main-thread dispatch guarantees for unsafe handlers** (UI, scripting)
- [ ] **Background job → main thread handoff helpers**
- [ ] **Debug assertions** for illegal cross-thread dispatch

### Entity & Gameplay Integration

- [ ] **Entity-scoped events** (`OnSpawn`, `OnThink`, `OnTouch`, `OnUse`, `OnDeath`)
- [ ] **World events** (`OnMapLoad`, `OnMapUnload`, `OnCheckpoint`)
- [ ] **Player events** (`OnConnect`, `OnDisconnect`, `OnInput`, `OnRespawn`)
- [ ] **Damage/combat events decoupled from direct calls**
- [ ] Replace hard-coded callbacks with event dispatch **where feasible**

### Networking & Replication

- [ ] **Define network-relevant events vs local-only events**
- [ ] Event → snapshot mapping rules (**what events affect state**)
- [ ] **Deterministic replay of events** (for demo/replay system)
- [ ] **Event filtering per client** (interest management hooks)
- [ ] **Security**: validate network-originated events strictly

### Scripting (Lua) Integration

- [x] **Expose event bus to Lua**
    - `Events.on(event, fn)` ✓
    - `Events.once(event, fn)` ✓
    - `Events.emit(event, data)` ✓
    - `Events.wait_for(event_name, timeout_ms)` ✓
- [ ] **Script-side event filters** (by entity, tag, distance, team, etc.) - Framework ready, needs filter implementation
- [x] **Coroutine-safe event waiting** (`wait_for_event`) ✓ Implemented as `Events.wait_for()`
- [x] **Script event error isolation** (handler failure does not kill bus) ✓ Error handling with isolation
- [x] **Hot-reload behavior** for subscribed script handlers ✓ `Lua_Events_HotReload()` implemented

### Tooling & Debugging

- [ ] **Event tracing** (enable/disable per category)
- [ ] **Event inspector UI**
    - live event stream
    - subscribers per event
    - queue depth / backlog
- [ ] Log events as **structured logs** (tie into structured logging TODO)
- [ ] Tracy/Profiler integration:
    - event dispatch zones
    - handler execution time
- [ ] **Replayable event capture** (for debugging/bugs)

### Persistence & Save/Load

- [ ] Define which events are **transient vs persistent**
- [ ] **Save/load behavior for scheduled events**
- [ ] **Versioned event payload schemas**
- [ ] **Backwards-compatibility** handling for old saves

### Configuration & Extensibility

- [ ] **Data-driven event definitions** (JSON/YAML for simple events)
- [ ] **Plugin/mod registration** of new event types
- [ ] **Soft-fail for unknown events** (warn, don’t crash)
- [ ] **Editor hooks** (Radiant emits events on selection, compile, etc.)

### Testing & Validation

- [ ] **Unit tests for event ordering and cancellation**
- [ ] **Stress tests** for high-frequency events
- [ ] **Determinism tests** (same inputs → same event stream)
- [ ] **Fuzz event payloads** (ties into security/fuzzing TODOs)

### Documentation

- [ ] **Event naming conventions & best practices**
- [ ] "When to use events vs direct calls" guide
- [ ] Examples:
    - damage flow
    - scripted encounter
    - UI reacting to gameplay
- [ ] **EDA + ECS interaction diagram**

- [ ] **Radiant TODOs**
    - Review `/tools/radiant` for outstanding technical debt
    - Extract and track unfinished items from code `TODO`/`FIXME` comments
    - Document Radiant-specific tooling or editor needs
    - Align Radiant codebase TODOs with this central tracker
    - Audit Radiant UI widgets for missing features & consistency
    - Implement automated tests for core Radiant tools
    - Improve error feedback and diagnostics in Radiant tools
    - Add support for custom user plugins in the Radiant editor
    - Upgrade Radiant scripting interface for better mod integration
    - Refactor key modules for maintainability and extensibility
    - Enhance Radiant documentation (extend SDK/API reference)
    - Link Radiant event system notifications to debug/trace panels
    - Review and optimize Radiant startup and resource loading times
    
    - Define MVP feature set for Radiant Editor
    - Establish user stories for editor workflows (map editing, entity placement, etc.)
    - Implement undo/redo system
    - Develop selection and manipulation tools (move, rotate, scale)
    - Integrate event-driven architecture into editor core
    - Create extensible property inspector for entities
    - Add layer and grouping support
    - Design UI/UX mockups and gather feedback
    - Implement grid/snapping and alignment features
    - Provide real-time preview and live editing capabilities
    - Integrate asset browser and drag-and-drop support
    - Set up hot-reload for scripting or plugins
    - Develop serialization and deserialization for editor state
    - Allow keyboard shortcuts and configurable keymaps
    - Write unit and integration tests for editor components
    - Perform regression testing with complex scenes
    - Stress-test loading/saving very large projects
    - Validate correct event emission when editing
    - Set up automated build & deployment pipeline for editor binaries
    - Document editor API and scripting interfaces
    - Create getting started and advanced usage guides
    - Gather and triage user feedback during testing/beta
