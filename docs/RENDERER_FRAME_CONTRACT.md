# Renderer Frame Contract

**Status:** Foundation Consolidation — production frame resource ownership and black-frame diagnostics.  
**Related:** [BLACK_FRAME_REGRESSION.md](BLACK_FRAME_REGRESSION.md) · [RENDERER_IDTECH7_SPRINT.md](RENDERER_IDTECH7_SPRINT.md) · `vk_black_frame.c` · `vk_frame_contract.c`

Tracks SceneHDR, depth, G-buffer attachments, OIT, weapon HDR, and post-chain sources across a frame. Pairs with Milestone 1 validation (`renderer_validate_frame`) and extended contract capture.

---

## Ownership

| Resource | Primary writer | Primary readers |
|----------|----------------|-----------------|
| SceneHDR (`vk.color_image`) | Opaque world, deferred composite, OIT resolve | Exposure, bloom, TAA, tonemap |
| SceneDepth | Main depth prepass / opaque draws | Hi-Z, deferred lighting, SSR, TAA |
| GBuffer albedo / normal / material | Deferred G-buffer fill | Deferred lighting, debug views |
| GBuffer lighting | Deferred lighting compute | Scene composite, reference lab |
| Velocity / temporal class | Main pass motion export | TAA, temporal reset policy |
| OIT accum / reveal | WBOIT / MBOIT passes | OIT resolve → SceneHDR |
| WeaponHDR / history | Architecture B weapon path | Weapon composite, TAA |
| Bloom / tonemap / final LDR | Post chain | Present |

`vk_frame_contract.c` records first/last writer, reader list, generation, and invalidation reason per slot (`vkFrameContractResource_t`).

---

## Data flow

```text
Opaque draws → SceneDepth + (optional) G-buffer fill
            → Deferred lighting → SceneHDR
Transparent / OIT → OIT buffers → resolve → SceneHDR
Weapon path → WeaponHDR → composite → SceneHDR
SceneHDR → exposure → bloom → tonemap → gamma → present
```

Each pass calls `vk_frame_contract_note_writer` / `vk_frame_contract_note_reader`. Black-frame diagnostics (`vk_black_frame.c`) maintain a parallel SceneHDR writer chain and draw-class counts for Milestone 1 checks.

System generations (cluster list, GPU scene, material buffer, shadow, probe, exposure) are snapshotted in `renderer_frame_status`.

---

## Buffer formats

| Slot | Typical format | Notes |
|------|----------------|-------|
| SceneHDR | `R16G16B16A16_SFLOAT` or swapchain HDR | Main color attachment |
| SceneDepth | `D32_SFLOAT` | Reversed-Z (near=1, far=0) |
| GBuffer0 albedo | `R8G8B8A8_UNORM` | Base color + flags (compact path) |
| GBuffer1 normal | `R16G16B16A16_SFLOAT` (scaffold) or octahedral in material | See [GBUFFER_2.md](GBUFFER_2.md) |
| GBuffer2 material | `R8G8B8A8_UNORM` | Roughness, metallic, AO |
| Velocity | `R16G16_SFLOAT` | Screen-space motion |
| OIT | FP16 accum + reveal | WBOIT production path |

---

## Lifecycle

1. **Register** — `vk_frame_contract_register()` adds console commands at renderer init (after `vk_black_frame_register`).
2. **Begin frame** — `vk_frame_contract_begin_frame()` clears per-frame write/read flags.
3. **Pass annotations** — writers/readers noted during `vk_frame_submit` spine order.
4. **Validate** — `renderer_validate_frame` (black-frame) + contract validate on demand or capture.
5. **Capture** — `renderer_capture_frame_contract` dumps snapshot JSON-style to console/log.
6. **Shutdown** — history cleared with renderer teardown.

Generations bump on resize, `vid_restart`, deferred G-buffer rebuild, and OIT attachment regen.

---

## Fallback behavior

- Missing SceneHDR handle → validation **FAIL** (black frame).
- Opaque draws with no SceneHDR writer → validation **FAIL**.
- Invalid or zero exposure when exposure generation active → validation **FAIL**.
- Depth writer not annotated → **WARN** only (depth often written outside noted chain).
- Read-without-write same frame on persistent attachments → **WARN** (stale read hint).

`r_forceMinimalScene 1` forces a known-good draw path for isolation.

---

## Debug commands

| Command | Role |
|---------|------|
| `renderer_validate_frame` | Milestone 1 checklist (SceneHDR, OIT gen, exposure, writers) |
| `renderer_frame_status` | Full contract snapshot: resources, generations, readers |
| `renderer_capture_frame_contract` | Persist contract snapshot for regression logs |
| `renderer_resource_status` | Live image handles, formats, extents |
| `renderer_draw_status` | Draw-class counts (opaque, forward, OIT, UI) |
| `renderer_capture_black_frame` | Trigger black-frame capture dump |
| `gbuffer_bandwidth` | G-buffer bytes/px scaffold vs compact |

Cvars: `r_frameOutputDebug`, `r_captureBlackFrame`, `r_forcePassColor`, `r_gbufferBandwidth`.

---

## Performance cost

Contract tracking is **CPU-only** string bookkeeping per annotated pass. Expected overhead: negligible (<0.05 ms/frame). Validation runs on console command or explicit capture — not every frame unless `r_captureBlackFrame` latches.

---

## Known limitations

- Not all depth writes are annotated yet; WARN path is intentional.
- Contract does not yet enforce GPU queue timeline ordering — pass registry spine is authoritative for ordering bugs.
- Weapon / Surf composite writers may share SceneHDR without duplicate-writer detection beyond first/last chain.
- Extended resources (visibility buffer, neural atlases) are out of scope for M1 contract slots.

---

## Next milestone hooks

- Wire `renderer_frame_status` fail count into CI soak scripts.
- Auto-validate after reference-lab scene transitions.
- Unify with `vk_pass_registry` stale-generation violations.
- Add generation parity checks for cluster / material SSBOs at frame boundary.

Regression: `tests/scripts/test_renderer_frame_contract.sh`
