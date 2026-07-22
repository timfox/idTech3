# WBOIT GPU Live Certification

Live operator certification for production **WBOIT** (`r_oit 1`) on real GPUs. Complements static CI gates — it does **not** replace them.

**Related:** [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) (algorithm + static gates) · [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md) (research tracks) · [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md) (fog-through-layers pass order)

---

## Certification levels

| Level | Name | Meaning |
|-------|------|---------|
| 0 | `NONE` | No certification progress recorded |
| 1 | `STATIC_GATES` | Static scripts / CI greps pass (`tests/scripts/test_wboit_*.sh`, `./scripts/oit_corruption_check.sh`) |
| 2 | `LIVE_BASIC` | At least one live B0–B7 case passed and exported |
| 3 | `LIVE_FULL` | All live B0–B7 cases passed with zero failures; session exported |
| 4 | `LIVE_SOAKED` | `oit_soak_wboit 30` completed with zero anomalies and no clear/resolve imbalance |
| 5 | `SPINE_1_1_CERTIFIED` | `LIVE_SOAKED` + `LIVE_FULL` on WBOIT production profile (mode 3 overlay or equivalent) |

Query current level:

```text
oit_certification_status
```

---

## Static scripts alone do NOT certify

Passing `tests/scripts/test_wboit_*.sh` only sets **`STATIC_GATES`**. That confirms source wiring, cvar defaults, and documentation contracts — not visual correctness on your GPU.

**Spine 1.1 / production certification requires:**

1. Static gates green in CI
2. Live B0–B7 operator matrix (`oit_certify_wboit begin` → pass/fail each case → `export`)
3. 30-minute soak with zero anomalies (`oit_soak_wboit 30`)
4. Validation-layer run clean (see checklist below)
5. Performance within budget placeholders (operator fill-in)

The engine prints explicitly: `STATIC_GATES alone is NOT Spine 1.1 certification`.

---

## Console commands

### Live case runner — `oit_certify_wboit`

```text
oit_certify_wboit begin          # start session, print case B0a
oit_certify_wboit pass           # mark current case PASS, advance
oit_certify_wboit fail <reason>  # mark FAIL, record anomaly
oit_certify_wboit next           # skip without recording
oit_certify_wboit repeat         # re-print current case hints
oit_certify_wboit status         # session progress + counters
oit_certify_wboit export         # write oit_cert_<session>.json; update LIVE_BASIC/FULL
oit_certify_wboit abort          # cancel active session
```

**Prerequisites:** `seta r_oit 1` (WBOIT). Recommended: `exec vulkan_overlay_oit_clustered.cfg` then `vid_restart`.

### Soak telemetry — `oit_soak_wboit`

```text
oit_soak_wboit 30               # 30-minute automated stress; exports oit_soak_<session>.csv
```

Cycles motion, lights, mode toggles, and fault injection phases. Samples once per second (120-frame history ring). Completion with `anomalies=0` and no clear/resolve imbalance sets **`LIVE_SOAKED`**.

### Status — `oit_certification_status`

Prints level, gate flags (`staticGates`, `liveBasic`, `liveFull`, `liveSoaked`), certified GPU/driver hashes, and `missing=` next step hint.

### Fog helper — `oit_fog_status`

Prints `r_oitFogMode`, density, accumulation/resolve fog ownership. See [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md).

### Anomaly capture

`r_oitCaptureOnError 1` (default) arms `oit_capture stages` when anomalies fire (cert fail, soak imbalance, postfx corruption hook).

---

## Live B0–B7 case mapping

Operator cases in `vk_oit_certify.c` map to soak tiers:

| Tier | Case IDs | Focus |
|------|----------|-------|
| **B0** | B0a, B0b, B0c | Static layered glass; alpha sweep; opaque readable through layers |
| **B1** | B1a, B1b, B1c | Camera slow/fast pan; inside translucent volume |
| **B2** | B2a, B2b, B2c | Moving glass; particle spawn/despawn; teleport recovery |
| **B3** | B3a–B3g | Even/odd extents; render scale 75%/50%; live resize; ultrawide; minimize/restore + `vid_restart` |
| **B4** | B4a, B4b, B4c | Unlit translucent; Forward+ many lights; contrast extremes |
| **B5** | B5a, B5b | 8–16 layer overdraw; 32–64 layer / fullscreen stress |
| **B6** | B6a, B6b, B6c | Bloom / fog / volumetrics (no double fog); MSAA/SMAA/TAA; weapon/UI |
| **B7** | B7a–B7e | OIT toggle recovery; forced allocation/extent/generation/cluster faults |

Each case prints `setup:` console hints on `begin` / `next`. Mark **`pass`** only when visual + `oit_status` counters look healthy.

---

## GPU matrix (operator fill-in)

Run live cert + soak on each target class. Leave blank until measured.

| Vendor | GPU | Driver | API | Resolution | MSAA | TAA | Cert level | Soak 30m | Notes |
|--------|-----|--------|-----|------------|------|-----|------------|----------|-------|
| NVIDIA | | | Vulkan 1.x | 1920×1080 | 0 | 0/1 | | | |
| NVIDIA | | | Vulkan 1.x | 2560×1440 | 0 | 0/1 | | | |
| NVIDIA | | | Vulkan 1.x | 3840×2160 | 0 | 0/1 | | | |
| AMD | | | Vulkan 1.x | 1920×1080 | 0 | 0/1 | | | |
| AMD | | | Vulkan 1.x | 2560×1440 | 0 | 0/1 | | | |
| AMD | | | Vulkan 1.x | 3840×2160 | 0 | 0/1 | | | |
| Intel | | | Vulkan 1.x | 1920×1080 | 0 | 0/1 | | | |
| Intel | | | Vulkan 1.x | 2560×1440 | 0 | 0/1 | | | |
| Intel | | | Vulkan 1.x | 3840×2160 | 0 | 0/1 | | | |

Export JSON + soak CSV per row; attach to release notes.

---

## Validation-layer run checklist

Before claiming `LIVE_FULL` or `SPINE_1_1_CERTIFIED`:

- [ ] Build with validation enabled (`r_vulkan_validation 1` or debug build)
- [ ] Complete B0–B7 with zero validation errors/warnings on OIT passes
- [ ] `oit_status` — no `unhealthy` flags; clear/resolve counts balanced per frame
- [ ] `r_oitDebug 14/15` — no unexpected tile bands (cheat)
- [ ] Odd extents B3b/B3c — no stale edge bands
- [ ] `oit_fog_status` — `doubleFogPrevention=enabled` when `r_oitFogMode>=1`
- [ ] Weapon path — gun clean after resolve (`B6c`, `r_temporalWeaponAfterTaa 1`)
- [ ] Export artifacts: `oit_cert_*.json`, `oit_soak_*.csv`

---

## Performance budget placeholders (operator fill-in)

Measure with `oit_perf` and frame time overlay. Targets are placeholders — replace with ship criteria.

### 1080p

| Profile | OIT clear+accum+resolve ms | Frame ms budget | Pass? |
|---------|---------------------------|-----------------|-------|
| LOW | | ≤ 16.7 (60 Hz) | |
| MEDIUM | | ≤ 16.7 (60 Hz) | |
| STRESS | | ≤ 33.3 (30 Hz min) | |

### 1440p

| Profile | OIT clear+accum+resolve ms | Frame ms budget | Pass? |
|---------|---------------------------|-----------------|-------|
| LOW | | ≤ 16.7 (60 Hz) | |
| MEDIUM | | ≤ 16.7 (60 Hz) | |
| STRESS | | ≤ 33.3 (30 Hz min) | |

### 4K

| Profile | OIT clear+accum+resolve ms | Frame ms budget | Pass? |
|---------|---------------------------|-----------------|-------|
| LOW | | ≤ 33.3 (30 Hz min) | |
| MEDIUM | | ≤ 33.3 (30 Hz min) | |
| STRESS | | ≤ 50.0 (20 Hz min) | |

Stress profile: `exec demo_wboit_stress_mode3.cfg` + dense translucent overdraw.

---

## Static gate scripts (CI)

These assert source contracts only:

| Script | Checks |
|--------|--------|
| `tests/scripts/test_wboit_live_cert_contract.sh` | `oit_certify_wboit`, B0 cases, cert levels in docs |
| `tests/scripts/test_wboit_soak_contract.sh` | `oit_soak_wboit`, history 120, CSV export |
| `tests/scripts/test_wboit_capture_on_error.sh` | `r_oitCaptureOnError`, `vk_oit_certify_note_anomaly` |
| `tests/scripts/test_wboit_certification_status.sh` | `oit_certification_status`, levels, static≠cert doc |
| `tests/scripts/test_wboit_fog_ownership.sh` | `oit_fog_status`, `r_oitFogMode`, pass order doc |
| `tests/scripts/test_wboit_fog_layers.sh` | `fogDensity` in `oit_accum.frag`, demo cfg |
| `tests/scripts/test_wboit_no_double_fog.sh` | double-fog prevention doc + shader path |
| `tests/scripts/test_wboit_fog_mode3.sh` | mode 3 + WBOIT + fog docs + overlay `r_oit 1` |

Plus existing: `test_wboit_defaults.sh`, `test_wboit_mode3.sh`, `test_wboit_lifecycle.sh`, etc.

---

## Workflow summary

```text
# 1. CI static gates
./tests/scripts/test_wboit_live_cert_contract.sh
# ... other test_wboit_*.sh

# 2. In-game live cert
exec vulkan_overlay_oit_clustered.cfg
vid_restart
oit_certify_wboit begin
# for each case: follow setup hints → pass | fail
oit_certify_wboit export

# 3. Soak
oit_soak_wboit 30

# 4. Verify
oit_certification_status
```

Expected final level: **`SPINE_1_1_CERTIFIED`** when all steps succeed on the target GPU.
