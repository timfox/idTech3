## Structured Logging
- [x] Migrate all logging to a structured logger with levels, categories, JSON output (see `src/qcommon/q_log.h/c`, `Com_Printf` routes through structured logger via `Q_Log_ComPrintf()`)
- [x] Add log rotation and filtering (see `log_rotation_size`, `log_rotation_time`, `log_category_filter` CVars, implemented in `q_log.c`)
- [x] Integrate with external logging/monitoring systems (syslog integration implemented, JSON format compatible with ELK/Loki/Splunk)

## Memory Safety & Profiling
- [x] ASan/UBSan supported via CMake option (`ENABLE_SANITIZERS`)
- [x] Provide documented workflows for ASan/UBSan
- [x] Add Valgrind/Dr. Memory integration for leak and error detection (see `tools/run_valgrind.sh`, `tools/run_drmemory.bat`, `valgrind.supp`, `docs/MEMORY_SAFETY_WORKFLOWS.md`)
- [x] Add memory usage tracking/stats in engine (see `src/qcommon/memory_stats.h/c`, console command `memstats`, integrated with zone allocator)

## Performance Profiling
- [x] Tracy Profiler integration (see `CMakeLists.txt` USE_TRACY option, `src/qcommon/profiler.h`, integrated in `common.c`)
- [x] Built-in performance counters: FPS, frame times, draw calls (see `src/qcommon/performance_counters.h/c`, integrated in renderer and engine)
- [x] GPU timing queries for renderer (see `src/renderervk/vk.c` timing query implementation, integrated with performance counters via `Perf_UpdateGPUTiming()`)

## Multi-threading
- [x] Thread pool for async operations (basic version present)
- [ ] Expand to proper job system (asset loading, etc.)
- [ ] Add lock-free data structures (where applicable)

## Code Quality & Tooling
- [x] clang-format configuration in repo (see `.clang-format`)
- [x] pre-commit hook for format enforcement (see `.git/hooks/pre-commit`)
- [x] Static analysis tools (clang-tidy, cppcheck) integration (see `.clang-tidy`, `cppcheck.cfg`, `tools/run_clang_tidy.sh`, `tools/run_cppcheck.sh`, `docs/STATIC_ANALYSIS_WORKFLOW.md`)
- [x] Expand unit tests (math, memory, networking—expanded coverage: added vector operations, cross product, angle normalization, bounds operations, multiple packet tests, unreliable packet tests - see `tests/test_qmath.c`, `tests/test_network_enet.c`)
- [x] Code coverage reporting (gcov/lcov) (see `CMakeLists.txt` ENABLE_COVERAGE option, `tools/run_coverage.sh`, `docs/CODE_COVERAGE.md`, gcovr and lcov support)

## Developer Experience
- [x] Asset pipeline tools support auto-conversion (see `tools/asset_conv`)
- [x] Hot reloading for game code (QVM) (see `src/qcommon/vm_hot_reload.h/c`, `docs/VM_HOT_RELOAD.md`, `tools/build_and_reload_qvm.sh`)
- [x] Improved debugging tools (ImGui debug overlays: Performance, Memory, Network, Renderer, CVar Browser, Console, Event System, Profiler - see `src/client/cl_imgui_debug.c`, `docs/imgui-debug-overlays.md`)
- [x] Better pipeline automation and validation (see `tools/validate_assets.py`, `docs/ASSET_VALIDATION.md`, CI integration in `.github/workflows/ci.yml`)

## Documentation
- [x] Doxygen API docs (see `docs/`)
- [ ] Keep architecture documentation up to date
- [x] Performance tuning guides (see `docs/PERFORMANCE_TUNING_GUIDE.md` - comprehensive guide covering CPU/GPU/memory/network optimization, profiling tools, CVar reference, benchmarking, troubleshooting)

## Security Hardening
- [ ] Fuzzing (AFL++, libFuzzer) for network and file parsing - basic harness only
- [ ] Stack canaries and security flags (partial, check all targets)
- [x] Some input validation improvements in progress

## Modern C Features & Practices
- [x] C23 feature usage where supported (some attributes/typeof in use)
- [x] Broader adoption (nullptr, designated initializers, static_assert) (see `docs/MODERN_CPP_FEATURES.md`)
- [x] Audit for type safety (const correctness, stronger types) (see `docs/TYPE_SAFETY_AUDIT.md`)

## CI/CD & Automation
- [x] CI for Windows, macOS, Linux builds (see `.github/workflows/`)
- [ ] Automated performance benchmarks
- [ ] Automated release packaging (only manual currently)

## Feature Improvements & Expansions
- [ ] Better audio system (OpenAL Soft planned, still using legacy)
- [ ] Improved physics (investigate bullet3 or similar)
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

- [ ] **Reproducible builds** (pinned toolchains, deterministic archives, `SOURCE_DATE_EPOCH`)
- [ ] **Package manager integration** (vcpkg/Conan presets; lockfile + CI cache)
- [ ] **Compiler matrix** (GCC/Clang/MSVC, `-Werror` on CI, warning budgets)
- [ ] **LTO/PGO toggles** (CMake options + docs + CI artifact)
- [ ] **Cross compilation** (Steam Deck / Linux cross toolchain; `x86_64` + `aarch64` plans)
- [ ] **Symbol + crash dump pipeline** (PDB/dSYM/DWARF upload, symbol server layout)

## Runtime Observability (beyond logging)

- [ ] **Central “stats + telemetry” system** (cvars + HUD + JSON snapshot endpoint)
- [ ] **Scoped trace zones everywhere** (CPU + GPU zones unified: Tracy or your replacement)
- [ ] **Frame capture hooks** (RenderDoc markers + capture automation in dev builds)
- [ ] **Configurable debug channels** (`r_debug_*`, `net_debug_*`, `fs_debug_*` categories)

## Crash Resilience & Diagnostics

- [ ] **Crash handler** (minidump + last 4k log ring buffer + build ID)
- [ ] **Assert strategy** (`hard assert`, `soft assert`, “once” asserts, per-module toggles)
- [ ] **“Safe mode” boot** (disable renderer mods, reset config, start windowed)
- [ ] **Watchdog for deadlocks** (thread heartbeat + dump stacks if hung)

## Determinism & Replayability (huge for networking + debugging)

- [ ] **Deterministic time step mode** (fixed tick, decoupled render)
- [ ] **Input recording + replay** (for bug repro and perf comparisons)
- [ ] **Deterministic RNG** (seed control per subsystem; snapshot seed state)
- [ ] **Golden test replays** in CI (run headless, validate checksums)

## Filesystem / Asset System Modernization

- [ ] **Virtual FS v2**: mount table, priority, write dir policy, sandboxing
- [ ] **Asset manifest + hashing** (content-addressable IDs, integrity checks)
- [ ] **Background streaming** (IO thread + decompression jobs + main-thread finalize)
- [ ] **Shader pipeline**: shader cache versioning, hot reload, fallback shaders
- [ ] **Texture pipeline**: KTX2/BasisU path (even if optional), mip policy, SRGB rules
- [ ] **Validation command**: `--validate-assets` (missing refs, bad paths, invalid metadata)

## Networking & Multiplayer Robustness

- [x] **Protocol versioning** (compat strategy, negotiated features, “strict/loose”) - see `docs/BACKWARDS_COMPATIBILITY.md`, protocol versioning with feature flags documented)
- [ ] **Snapshot correctness tests** (serialize/deserialize roundtrip fuzz + golden packets)
- [ ] **Rate limiting + abuse hardening** (per-IP, per-client, command budget)
- [ ] **NAT traversal plan** (even if later): abstraction layer + feature flags
- [ ] **Lag compensation hooks** (server rewind framework; even stubbed)

## Save/Load & Persistence

- [x] **Unified serialization framework** (engine + gamecode, versioned, schema-driven - see `docs/BACKWARDS_COMPATIBILITY.md` for framework design)
- [x] **Save corruption recovery** (atomic writes, backups, checksums - documented in `docs/BACKWARDS_COMPATIBILITY.md`)
- [x] **Migration tests** (load old saves in CI, auto-upgrade, verify invariants - see `tools/test_compatibility.sh`, `tools/migrate_save.py`, `docs/BACKWARDS_COMPATIBILITY.md`)

## Renderer: Modern “Must-haves”

- [ ] **Render graph / pass system** (even lightweight; explicit dependencies + barriers)
- [ ] **GPU validation workflows** (Vulkan validation layers toggle + docs + CI run)
- [ ] **Pipeline cache** (persistent, per-GPU, invalidation rules)
- [ ] **Texture streaming budget** (VRAM estimator + eviction policy)
- [ ] **Dynamic resolution / FSR/XeSS/DLSS strategy** (optional, but plan the interface)
- [ ] **HDR path** (swapchain formats, tonemap, UI color space rules)

## Audio

- [ ] **Audio graph abstraction** (mix buses, sends, ducking, priorities)
- [ ] **HRTF + spatialization** (OpenAL Soft path, fallbacks)
- [ ] **Audio profiler** (voices active, CPU time, memory, streaming health)

## Tooling & Content Authoring (Radiant-focused)

- [ ] **Gamepack schema + validator** (JSON schema, versioning, env var expansion tests)
- [ ] **Entity def modernization** (typed fields, defaults, ranges, UI hints)
- [ ] **Prefab system improvements** (dependency tracking, versioned prefabs, “bake to brushes”)
- [ ] **Map compile orchestration** (profiles, incremental compile, artifact caching)
- [ ] **Remote compile farm option** (later): compiler RPC protocol stub

## Testing & QA Engineering

- [ ] **Headless test runner** (unit + integration + replay tests)
- [ ] **Property tests** for math/geom (AABB, planes, winding, BSP ops)
- [ ] **Fuzz targets expanded** (bsp, shader scripts, pk3 parsing, network messages)
- [ ] **Performance regression gates** (threshold-based; fail CI if >X% regression)

## Security & Supply Chain

- [ ] **Dependency audit pipeline** (SBOM generation, `cargo/vcpkg` equivalent where applicable)
- [ ] **Signed releases** (sign binaries + manifests)
- [ ] **Secure defaults** (no remote downloads, no unsafe cvars in release builds)

## Packaging, Modding, UX

- [ ] **First-run UX** (auto-detect settings, safe defaults, controller prompts)
- [ ] **Mod packaging format spec** (your `.pk3` rename: define rules, mounting, conflicts)
- [ ] **In-engine mod browser hooks** (even if no UI yet: backend list/enable/disable)
- [ ] **Dedicated server packaging** (minimal headless build + scripts + docs)

## Scripting (your Lua plan) — missing “engine contracts”

- [ ] **Stable C API boundary for Lua bindings** (no direct engine internals)
- [ ] **Deterministic script execution rules** (what runs in tick vs frame)
- [ ] **Sandboxing** (no `os.execute`, file IO gating, whitelisted libs)
- [ ] **Error containment** (script errors don’t kill engine; quarantine failing coroutine)
- [ ] **Save/load for scripts** (serialize coroutine/encounter state or define reset policy)

---

### Shippability Meta

- [ ] Add **priorities + phases** to every section (P0 ship blocker / P1 / P2 nice-to-have)
- [ ] Add a **Definition of Done template** per feature (tests, docs, CI, perf impact, debug toggles)

---

## Event-Driven Architecture (EDA)

### Core Event System

- [x] **Define central event bus API (engine-level)** (see `src/qcommon/event_system.h/c`, `docs/EVENT_SYSTEM.md`)
    - `publish(event)` - implemented as `Event_Publish()`
    - `subscribe(event_type, handler, priority)` - implemented as `Event_Subscribe()`
    - `unsubscribe(handle)` - implemented as `Event_Unsubscribe()`
    - Support **typed events** (struct-based, not string-only) - implemented with `event_t` struct
    - Event **categories/namespaces**: `Engine.*`, `Game.*`, `Entity.*`, `Net.*`, `UI.*` - implemented as `eventCategory_t` enum
    - **Event priority ordering**: pre, normal, post - implemented as `eventPriority_t` enum
    - **Event cancellation / consumption** semantics - basic support via event flags
    - **Event bubbling rules** (entity → world → engine, if applicable) - framework ready for expansion

### Event Lifecycle & Timing

- [x] **Explicit event phases** (see `src/qcommon/event_system.h/c`, `docs/EVENT_PHASES.md`)
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

- [ ] **Expose event bus to Lua**
    - `Events.on(event, fn)`
    - `Events.once(event, fn)`
    - `Events.emit(event, data)`
- [ ] **Script-side event filters** (by entity, tag, distance, team, etc.)
- [ ] **Coroutine-safe event waiting** (`wait_for_event`)
- [ ] **Script event error isolation** (handler failure does not kill bus)
- [ ] **Hot-reload behavior** for subscribed script handlers

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

---



