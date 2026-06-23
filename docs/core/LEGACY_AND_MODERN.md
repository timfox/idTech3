# Legacy intact, modern by default

This fork **does not trade compatibility for speed**. The 2026 layout and build profiles are **additive**: legacy mods and paths keep working; modern builds compile less code by default and expose clearer structure.

## Dual promise

| Legacy (unchanged) | Modern (2026) |
|--------------------|-----------------|
| QVM + native module loading ([COMPATIBILITY.md](../COMPATIBILITY.md)) | `IDTECH3_PROFILE=game` default — faster clean builds |
| Retail Q3 / OpenArena-style `.pk3` stacks | Vulkan PBR, Forward+, optional RTX |
| `src/qcommon`, `src/client`, … **forwarding shims** (one release) | **`engine/`, `runtime/`, `modules/`** canonical physical tree (Phase 5c) |
| Traps, cvars, network protocol surface | Explicit CMake manifests + extension gates |
| `examples/`, `BUILD_EXAMPLE_DEMO_GAME` | `samples/`, `BUILD_SAMPLES_DEMO_GAME` (deprecated alias kept) |
| `./scripts/compile_engine.sh vulkan` | Same command; profile=`game` unless you pass `core` / `full` |

**Rule:** Nothing in [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md) removes a capability from the tree. Opt-out is **compile-time** (`core` / `game` / `full`), not delete.

## Performance without breaking mods

Build profiles shrink **what you compile**, not what you can ship:

| Profile | Compile-time win | Mod/runtime parity |
|---------|------------------|-------------------|
| `core` | Fewest TU — CI / dedicated server / Q3 smoke | QVM path validated; no open-world / research / AI middleware in **binary** |
| `game` | ~25–40% fewer extension TU vs kitchen-sink | Recommended SP conversion; open world, nav, USD |
| `full` | Same TU count as pre-2026 defaults | All generative, neural renderer pack, research |

Enable anything from `full` on a `game` build: `-DUSE_RESEARCH_EXTENSIONS=ON`, `-DIDTECH3_PROFILE=full`, or per-flag `-DUSE_*=ON`. See [BUILD.md](../../BUILD.md).

## Legacy invariants (must not regress)

1. **Canonical sources** live under `engine/`, `runtime/`, `modules/`, `extensions/`, `renderers/`, `third_party/`; **`src/*` shims** forward for one release (Phase 5c).
2. **CMake manifests** use `IDTECH3_DIR_*` for includes and explicit client lists; `idtech3_legacy_src()` keeps AUX strip/append paths stable (Phase 5b).
2. **`./scripts/q3_openarena_compat_check.sh`** passes on **`core`** profile builds.
3. **QVM** remains in `vm.c`; native-first loading unchanged.
4. **Public game API** — traps, `g_*` where exposed to QVM, pk3/fs layout — stable per [CLAUDE.md](../../CLAUDE.md) constitution.
5. **Deprecation shims** — one release minimum ([DEPRECATION_POLICY.md](../DEPRECATION_POLICY.md)).

CI wiring: `test_legacy_intact.sh`, `test_repository_layout_2026.sh`, profile matrix in `.github/workflows/build.yml`.

## For mod authors

- **Classic QVM mod:** build or download **`core`** or **`game`**; drop `.pk3` in `base/` as always. **`cl_autoGraphicsProfile 1`** (default) loads **`classic_baseq3.cfg`** for retail baseq3 + **`cgame.qvm`**.
- **Full conversion:** default **`game`** profile; native cgame auto-loads **`modern_native.cfg`** when **`cl_autoGraphicsProfile 1`**. Override with **`cl_autoGraphicsProfile 0`** or manual cfgs.
- **Research / ML / neural renderer:** `./scripts/compile_engine.sh vulkan full` or explicit `-DUSE_*=ON`.

## Related docs

- [COMPATIBILITY.md](../COMPATIBILITY.md) — QVM, platform matrix
- [ENGINE_MODULE_MANIFEST.md](../ENGINE_MODULE_MANIFEST.md) — what each profile compiles
- [REPOSITORY_LAYOUT_2026.md](REPOSITORY_LAYOUT_2026.md) — folder map
- [BRANCHES.md](../BRANCHES.md) — vanilla / chocolate / layercake layers
