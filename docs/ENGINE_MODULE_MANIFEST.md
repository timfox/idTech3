# Engine module manifest

Authoritative inventory for the **2026 build profile diet**. Every optional source must appear here with its CMake gate, tier, and validation script.

**Build profiles** (`IDTECH3_PROFILE`, default `game`):

| Profile | Purpose |
|---------|---------|
| `core` | Q3/OA compat + Vulkan forward; no open world / research / generative |
| `game` | SP conversion default — open world, nav, FreeUSD |
| `full` | Kitchen-sink parity with pre-2026 `./scripts/compile_engine.sh vulkan` |
| `research` | `full` + explicit research tooling emphasis (same gates as `full`) |

Profile bundles: [`cmake/IdTech3Profile.cmake`](../cmake/IdTech3Profile.cmake), [`cmake/profiles/`](../cmake/profiles/).

---

## Tier legend

| Tier | Meaning |
|------|---------|
| **core** | Always linked in every profile |
| **module** | World/gameplay stack; gated by feature flags |
| **extension** | Research, generative ML, neural renderer pack |

---

## Qcommon — research extensions

Gate: **`USE_RESEARCH_EXTENSIONS`** (OFF in `core`/`game`, ON in `full`/`research`).

CMake: [`cmake/IdTech3QcommonExtensions.cmake`](../cmake/IdTech3QcommonExtensions.cmake) → `idtech3_append_research_qcommon_sources()`.

| Path | Tier | Runtime cvar / notes | Test |
|------|------|----------------------|------|
| `src/extensions/research/vuda/` | extension | `r_vuda`, `cl_vuda` | `tests/scripts/test_vuda.sh` |
| `src/extensions/research/vksplat/` | extension | `r_vksplat` | `tests/scripts/test_vksplat.sh` |
| `src/extensions/research/curast/` | extension | `r_curast` | `tests/scripts/test_curast.sh` |
| `src/extensions/research/infernux/` | extension | Infernux batch | `tests/scripts/test_python.sh` |
| `src/extensions/research/mimir/` | extension | `r_mimir` | `tests/scripts/test_mimir.sh` |
| `src/extensions/research/iris/` | extension | `r_iris` | `tests/scripts/test_iris.sh` |
| `src/extensions/research/radiusfps/` | extension | `cl_radiusfps_*` | `tests/scripts/test_radiusfps.sh` |
| `src/extensions/research/gccfer/` | extension | `cl_gccfer_*` | `tests/scripts/test_gccfer.sh` |
| `src/extensions/research/dax/` | extension | `cl_dax_*` | `tests/scripts/test_dax.sh` |
| `src/extensions/research/x3dpra/` | extension | `cl_x3dpra_*` | `tests/scripts/test_x3dpra.sh` |

Optional CUDA: `USE_RADIUSFPS_CUDA`, `USE_VUDA`, `USE_MIMIR_CUDA` (platform + profile dependent).

---

## Qcommon — open world module

Gate: **`USE_OPEN_WORLD`** (OFF in `core`, ON in `game`/`full`/`research`).

CMake: `idtech3_append_open_world_qcommon_sources()`.

| Path | Tier | Runtime cvar / notes | Test |
|------|------|----------------------|------|
| `src/world/world_district.cpp` | module | `r_district` | `tests/scripts/test_districts.sh` |
| `src/world/world_open.cpp` | module | `r_openWorld` | `tests/scripts/test_openworld.sh` |
| `src/world/world_residency.cpp` | module | residency hooks | `tests/scripts/test_openworld_residency.sh` |
| `src/world/sector_graph.cpp` | module | sector graph | `tests/scripts/test_graph_compute.sh` |
| `src/world/fog_biology.cpp` | module | `r_fogBiology` | `tests/scripts/test_fog_biology.sh` |
| `src/world/genetic_gan.cpp` | module | genome slots | `tests/scripts/test_genetic_gan.sh` |
| `src/world/world_proc.cpp` | module | `r_proc` | `tests/scripts/test_proc.sh` |
| `src/qcommon/cluster_graph.cpp` | module | cluster graph | `tests/scripts/test_cluster_graph` (unit) |
| `src/qcommon/cm_stream_merge.c` | module | `cm_streamMerge` | `tests/scripts/test_cm_stream_merge.sh` |
| `src/qcommon/com_openworld_smoke.c` | module | smoke harness | `tests/scripts/test_openworld_runtime.sh` |

---

## Client — generative extensions

Gate: **`USE_FLUX`**, **`USE_TRELLIS`**, **`USE_GENETIC_GAN`** (from profile).

CMake: [`cmake/client/ClientExtensionSources.cmake`](../cmake/client/ClientExtensionSources.cmake).

| Path | Tier | Gate | Test |
|------|------|------|------|
| `src/extensions/generative/cl_flux.c` | extension | `USE_FLUX` | FLUX docs / manual |
| `src/extensions/generative/cl_trellis.c` | extension | `USE_TRELLIS` | `tests/scripts/test_trellis` (if present) |
| `src/extensions/generative/cl_genetic_gan.c` | extension | `USE_GENETIC_GAN` | `tests/scripts/test_genetic_gan.sh` |
| `src/extensions/generative/cl_ml_worker.c` | extension | `USE_GENETIC_GAN` | same |
| `src/extensions/generative/cl_generative.c` | extension | any generative flag | wiring grep in tests |
| `src/client/cl_district.cpp` | module | `USE_OPEN_WORLD` | `test_districts.sh` |
| `src/client/cl_openworld.cpp` | module | `USE_OPEN_WORLD` | `test_openworld*.sh` |
| `src/client/cl_proc.cpp` | module | `USE_OPEN_WORLD` | `test_proc.sh` |

Headers: `src/extensions/generative/*.h`, `src/client/cl_flux.h`, `src/client/cl_trellis.h`.

---

## Vulkan renderer

Gate: **`USE_EXPERIMENTAL_RENDERERS`** (neural pack + extension `.c` strip when OFF).

CMake: [`cmake/renderers/VulkanExtensionSources.cmake`](../cmake/renderers/VulkanExtensionSources.cmake), [`cmake/renderers/VulkanCoreSources.cmake`](../cmake/renderers/VulkanCoreSources.cmake).

| Manifest | Tier | Gate |
|----------|------|------|
| `VK_EXPERIMENTAL_RENDERER_SRCS` | extension | `USE_EXPERIMENTAL_RENDERERS` |
| `VK_PROFILE_EXTENSION_SRCS` | extension | `USE_EXPERIMENTAL_RENDERERS` |
| Forward+, deferred, BSP, fonts, volumetrics | core | always (Vulkan backend) |
| `vk_experimental_renderer_stubs.c` | core | used when experimental OFF |

RTX: **`USE_VULKAN_RTX`** (separate from experimental pack).

---

## Always-on core (do not gate without manifest update)

- `src/qcommon/*` (except gated net/world/research above)
- `src/client/*` except extension strips in ClientExtensionSources
- `src/server/*`, `src/game/*`, `src/audio/*`, `src/platform/*`
- `src/renderers/common/*`, `src/renderers/vulkan/*` minus extension manifests

---

## Governance

- New unconditional `list(APPEND QCOMMON_SRCS` / `CLIENT_SRCS` in root `CMakeLists.txt` is **forbidden** — use cmake module macros; CI: `scripts/ci/audit_unconditional_sources.sh`.
- Physical moves require one-release include shims — see [`docs/DEPRECATION_POLICY.md`](DEPRECATION_POLICY.md).
- PR checklist: manifest updated when sources move or gates change.

---

## Deferred (Phase 5+)

- Top-level `engine/`, `runtime/`, `modules/` physical roots — [REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md)
- `src/external/` → `third_party/` rename
- `USE_GAME_AI_MIDDLEWARE` compile gate — scaffold in `cmake/modules/ClientGameAiSources.cmake` (needs stub headers for `g_lua_bindings.c`)
- MSVC project codegen from CMake
