# HDR Pipeline

**Status:** Foundation Consolidation — post-chain stage ownership and composition order.  
**Source:** `vk_hdr_pipeline.c` · **Validation:** `vk_black_frame.c`, `vk_frame_end.c`

---

## Ownership

| Stage | Typical writer | Output |
|-------|----------------|--------|
| Scene | World + composite | SceneHDR |
| Exposure | Auto-exposure / metering | Adapted exposure uniform |
| Bloom | Threshold + blur | Bloom texture |
| Tonemap | ACES / Reinhard / custom | LDR HDR intermediate |
| Gamma | sRGB encode | Display-ready |
| Present | Swapchain blit / UI | Final LDR |

`vk_hdr_pipeline_note_stage()` records last pass per stage; `vk.postChainLastWriter` mirrors spine intent.

---

## Data flow

```text
SceneHDR (world composite complete)
  → exposure adapt (vk.adaptedExposure)
  → bloom extract + blur → composite add
  → tonemap (HDR → LDR range)
  → gamma / color grading
  → UI / video overlay
  → present
Weapon / Surf may inject pre-exposure HDR (Architecture B) — see frame contract.
```

Composition order must match [BLACK_FRAME_REGRESSION.md](BLACK_FRAME_REGRESSION.md) Milestone 1 checks (exposure finite, tonemap after bloom).

---

## Buffer formats

| Stage | Format |
|-------|--------|
| SceneHDR | `R16G16B16A16_SFLOAT` typical |
| Bloom | half-res or full FP16 chain |
| Tonemap source | matches bloom composite target |
| Final LDR | swapchain `B8G8R8A8` or `R16G16B16A16` HDR display |

---

## Lifecycle

1. `vk_hdr_pipeline_register()` — `r_hdrStageDebug`, `hdr_pipeline_status`.
2. `vk_hdr_pipeline_begin_frame()` — clear stage writers.
3. Post passes call `vk_hdr_pipeline_note_stage(stage, passName)`.
4. Frame end validates exposure + writer chain via black-frame.

---

## Fallback behavior

- Exposure failure → validation FAIL in `renderer_validate_frame`.
- Bloom disabled → tonemap reads SceneHDR directly.
- `r_hdrStageDebug 0` — no overlay; production path unchanged.
- Safe profile configs skip experimental grading.

---

## Debug commands

| Cvar / command | Role |
|----------------|------|
| `hdr_pipeline_status` | Per-stage last writer, adapted exposure, post chain writer |
| `r_hdrStageDebug` | 1 scene, 2 exposure, 3 bloom, 4 tonemap, 5 gamma, 6 full chain overlay |
| `renderer_validate_frame` | Exposure + SceneHDR checks |

---

## Performance cost

| Stage | 1080p typical |
|-------|---------------|
| Exposure | <0.05 ms (compute reduce) |
| Bloom | 0.3–1.5 ms |
| Tonemap + gamma | 0.1–0.3 ms |
| Debug overlay | +0.1 ms when cheat on |

---

## Known limitations

- Stage tracking relies on explicit `note_stage` calls — not all legacy passes annotated yet.
- Surf / weapon HDR merge timing documented in frame contract, not duplicated here.
- No HDR10 metadata path in Foundation Consolidation.

---

## Next milestone hooks

- Auto-annotate remaining post passes in spine registry.
- Reference lab `VK_REFLAB_SCENE_HDR_PRESENTATION` automated capture list.
- CI: `hdr_pipeline_status` writer non-empty for each enabled stage.

Regression: `tests/scripts/test_hdr_composition.sh`
