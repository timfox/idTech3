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

## OOP / Entity Architecture
- [x] Decide OOP strategy for game VM: use C++ for new gamecode, keep a stable C ABI shim for engine/VM boundaries and QVM compatibility
- [ ] Document C/C++ boundary rules (extern "C", POD layouts for net/save structs, no RTTI/exceptions across boundary)
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

