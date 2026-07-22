# Shadow Contract

**Status:** Foundation Consolidation — GPU shadow atlas / CSM consumer tracking.  
**Source:** `vk_shadow_contract.c` · **Virtual shadow:** `vk_vshadow.c`

---

## Ownership

| Resource | Owner | Consumers |
|----------|-------|-----------|
| CSM depth cascades | Shadow map pass | Deferred, Forward+, SSR |
| Virtual shadow pages | `vk_vshadow.c` | Page table sampling |
| **`GpuShadowRecord`** | `vk_shadow_contract.c` | Tracks slot, cascade, generation, consumers |

Each shadow sample path registers as consumer via `vk_shadow_contract_note_consumer`.

---

## Data flow

```text
Sun / dlight shadow casters → depth atlas (CSM or vshadow pages)
  → vk_shadow_contract_alloc(slot, cascade)
  → consumers note: deferred_lighting, forward_plus, SSR, ...
Sampling uses cascade index + PCF / page table (vshadow)
r_shadowConsumerDebug → false-color consumer / generation mismatch
Fallback: r_vshadowFallbackCsm 1 keeps CSM when pages not resident
```

---

## Buffer formats

**GpuShadowRecord**:

| Field | Type | Purpose |
|-------|------|---------|
| slot, cascade | `uint32` | Atlas slot index |
| generation | `uint32` | Bump on resize / map change |
| extentW, extentH | `uint32` | Cascade resolution |
| depthHandle, colorHandle | `uint64` | VkImage handles |
| consumer | `char[32]` | Last registered consumer name |
| allocated | `qboolean` | Slot active |

Depth formats: `D32_SFLOAT` or `D16` atlas tiles. Vshadow uses page table + companion depth fill.

GPU SSBO: packed `GpuShadowGpuRecord` (160 B × 16 slots) uploaded via `vk_shadow_contract_upload_ssbo()` after CSM finalize; bind with `vk_shadow_contract_ssbo()`.

Deferred lighting binds SSBO at set0 binding 10 + sun CSM atlas at binding 11 (`shadow_contract.glsl`); cascade sample uses Forward+-matched depth compare (`ndc.z*0.5+0.5`, `compare <= map`) and atlasScaleBias tile UV. Enabled when `r_pbrSunShadow 1`. Forward+ continues to use UBO cascade rows and notes consumer `forward_plus`.

---

## Lifecycle

1. `vk_shadow_contract_register()` — `r_shadowConsumerDebug`, `shadow_status` command.
2. `vk_shadow_contract_begin_frame()` — per-frame consumer reset.
3. Shadow pass allocates records; lighting passes note consumption.
4. Map load / `vid_restart` bumps generation; stale samples must check gen.
5. Vshadow: claim_dirty → depth-fill → mark resident (`vshadow_status` counters).

---

## Fallback behavior

- `r_vshadowFallbackCsm 1` (default) — CSM sampling when virtual pages not ready.
- Missing shadow map → unshadowed direct light (no crash).
- Consumer generation mismatch → debug WARN; sampling uses best available cascade.
- `r_shadowConsumerDebug 1–3` — visualize consumer binding / stale gen.

---

## Debug commands

| Command / cvar | Role |
|----------------|------|
| `shadow_status` | Active records, generations, consumers |
| `vshadow_status` | Virtual shadow page residency counters |
| `r_shadowConsumerDebug` | 0 off, 1 consumer ID, 2 generation heat, 3 stale warn |

---

## Performance cost

Contract tracking: CPU-only, negligible. CSM: 1–4 extra depth passes (GPU bound). Vshadow: amortized page fill — see [RASTER_ULTRA docs](RASTER_ULTRA_1.6.md).

---

## Known limitations

- `GpuShadowRecord` tracks last consumer only — not a full multi-consumer list yet.
- Entity shadows / dlight shadows use separate atlases — not all unified in contract.
- Stale detection console-only; no auto-fallback in release builds.

---

## Next milestone hooks

- Multi-consumer ring buffer per cascade.
- Auto-bump generation on `r_map` / world config swap.
- CI: `shadow_status` generation match after reference lab shadow scene.

Regression: `tests/scripts/test_shadow_consumer_parity.sh`
