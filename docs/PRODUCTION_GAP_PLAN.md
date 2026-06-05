# Production Gap Plan (SP-first)

Living checklist for the **single-player full-conversion** track. Master plan lives in Cursor; this doc is the repo hub.

## Phase A — SP vertical slice

| Item | Status | How to verify |
|------|--------|----------------|
| Demo pk3: sound events | Done | `snd_playevent ui_click` with `fs_game idtech3_demo` |
| Demo pk3: i18n | Done | `com_loc_language en`; keys in `examples/demo_game/loc/en.loc` |
| Demo SP cfg | Done | `demo_sp_slice.cfg` — TAA, animgraph, `phys_character` |
| Demo physics cfg | Done | `demo_physics.cfg` — middleware + `phys_status` |
| Editor contract | Done | [RADIANT.md](RADIANT.md#editor-contract-checklist) |
| pk3.sig doc | Done | SHA-256 sidecar — [MOD_SDK.md](MOD_SDK.md#security--shipping) |
| Hero TAA + decals | Done | `r_temporalCustomShaderMotion 1`; decal stable frames in `tr_decal_props.c` |
| GPU golden Tier B | Capture cfg | `gpu_golden_capture.cfg` in demo pk3; [GPU_GOLDEN_TIER_B.md](GPU_GOLDEN_TIER_B.md) |

### Full SP map layout

```
YourInstall/
├── idtech3
├── base/                 # licensed compatible pk3s or minimal bootstrap
├── yourmod/              # your conversion mod (fs_game)
│   ├── *.pk3
│   ├── loc/en.loc
│   ├── sound/soundevents.txt
│   └── animgraph/
└── idtech3_demo/         # optional engine feature demo
    └── idtech3_demo.pk3
```

Launch: `./idtech3 +set fs_game yourmod +map yourmap` with `demo_sp_slice.cfg` equivalents in mod `autoexec.cfg`.

## Phase B — Persistence + hygiene

| Item | Doc / script |
|------|----------------|
| Save/load v1 | `Engine.Save.*` — [LUA_API.md](LUA_API.md); JSON `save/engine_slot_N.json` |
| loc CI | `scripts/check_loc_keys.sh` |
| Crash opt-in | [CRASH_REPORTING.md](CRASH_REPORTING.md); `tests/scripts/test_crash_report.sh` |
| Asset validate | `scripts/validate_assets.sh` |

## Phase C — World fidelity

| Item | Notes |
|------|--------|
| Temporal | `r_taa`, `r_temporalCpuSkinPrev`, transparents: enable `r_temporalCustomShaderMotion` for billboard/decals |
| FSR2 / upscale | `r_upscale` — SP preset in `demo_sp_slice.cfg` comments |
| Sector stream | `cm_stream 1`, `cl_sectorPrefetch 1`, `sv_sectorURL` |
| Retarget | [ANIMGRAPH.md](ANIMGRAPH.md) + `scripts/retarget_skel.py` |
| Neural Dynamic GI | Experimental — [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md); `r_ndgi`, `ndgi/<map>.ndgi` |
| Neural Irradiance Volume | Experimental — [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md); `r_niv`, G-buffer + `niv/<map>.niv` |
| Neural Image Space Tessellation | Experimental — [NEURAL_IMAGE_SPACE_TESSELLATION.md](NEURAL_IMAGE_SPACE_TESSELLATION.md); `r_nist`, G-buffer silhouette post-process |
| Neural visibility / vertex GI / RenderFormer | Experimental — [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md) (`r_nvc`, `r_vfgi`, `r_renderformer`) |
| Wavefront path experiment (screen) | Experimental — [WAVEFRONT_PATH_TRACING.md](WAVEFRONT_PATH_TRACING.md); `r_wpt` compute proxy |
| Path trace arch compare (RTX) | Experimental — [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md); `r_pathtrace`, `r_pathtrace_arch` megakernel/wavefront |

Full phased roadmap: **[NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md)** (Phase 1–3).

## Phase D — Platform (gated)

See [PLATFORM_GATED.md](PLATFORM_GATED.md). No work until product SKU requires store SDKs / native Metal / live MP.

## Validation

```bash
./scripts/compile_engine.sh vulkan demo
cd build-vk-Release && ctest -R 'smoke|renderer_regression|gpu_golden|test_demo_game_pk3|test_validate_assets|test_check_loc|test_crash_report|test_engine_save'
./scripts/renderer_regression_check.sh
```

## References

- [MOD_SDK.md](MOD_SDK.md) — replication template, traps
- [API_STABILITY.md](API_STABILITY.md) — semver
- [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md) — bootstrap pk3
- [ANIMGRAPH.md](ANIMGRAPH.md) — graph JSON
- [AUDIO_WWISE_PARITY.md](AUDIO_WWISE_PARITY.md) — mixer buses

---

## Appendix — Platform backlog (gated)

Tracked here for visibility; implementation waits on [PLATFORM_GATED.md](PLATFORM_GATED.md).

### Mod platform P0

| Item | Notes |
|------|--------|
| `requires_engine` in `gameinfo.txt` | Warn if `com_engine_api` &lt; required (see `FS_ParseRequiresEngine`) |
| `pk3.sig` SHA-256 | [MOD_SDK.md](MOD_SDK.md#security--shipping) |
| Engine.Save JSON v1 | `save/engine_slot_*.json`; `test_engine_save.sh` |
| Sector prefetch | `cm_stream` + `sv_sectorURL` + client CURL handler |

### Live MP P0 (deferred)

| Item | Notes |
|------|--------|
| `sv_auth.c` | Documented stub until live SKU |
| Signed pure + interest cull | `sv_pureSigned`, `sv_interestMaxDist` already exist |
