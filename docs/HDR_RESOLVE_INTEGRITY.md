# HDR Resolve Integrity (Phase 2.4)

**Status:** Frozen production resolve chain for WBOIT → SceneHDR.  
**Code:** `vk_hdr_resolve_contract.h` / `vk_hdr_resolve_contract.c`  
**Commands:** `hdr_resolve_status` (alias `oit_resolve_status`), `hdr_resolve_validate`  
**Version:** `HDR_RESOLVE_CONTRACT_VERSION` **1** + `contractHash`

Parent: [COLOR_PIPELINE.md](COLOR_PIPELINE.md). WBOIT math: [WBOIT_CONTRACT.md](WBOIT_CONTRACT.md). Fog: [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md).

---

## Frozen policy

| Rule | Value |
|------|-------|
| Resolve color space | `SCENE_LINEAR_HDR` |
| Before exposure / tonemap | **yes** |
| Empty accum / zero coverage | **preserve opaque** (never black) |
| Opaque base | `fog_scene` copy of SceneHDR **before** accum |
| Second full-screen fog on resolve | **forbidden** when `r_oitFogMode≥1` |
| Opaque / accum / reveal extents | must match |
| OIT attachment vs descriptor gen | must match |
| Fog-scene copy this frame | required immediately before resolve |

---

## Resource generations

| Counter | Bumped when |
|---------|-------------|
| `sceneHdrGeneration` | SceneHDR (`color_image`) recreate |
| `depthGeneration` | Depth attachment recreate |
| `fogSceneGeneration` | Each successful `vk_copy_color_to_fog_scene` |

`hdr_resolve_status` prints all three plus `fogCopiedThisFrame` and OIT gens.

---

## Resolve equation (unchanged)

```text
C_opaque = fog_scene  # scene-linear, already fogged if froxel ran before OIT
if empty(accum): C_out = C_opaque
C_avg = accum.rgb / max(accum.a, eps)
C_out = C_avg * (1 - reveal) + C_opaque * reveal
```

Shader: `oit_resolve.frag`. Pre-resolve gate: `vk_hdr_resolve_runtime_validate(qtrue, …)`.

---

## Soft particles (Phase 2.3.3)

GPU soft splat (`gp_soft_splat.comp`) linearizes opaque depth with `Depth_LinearizeReversedZ` from `depth_view.glsl` — same positive view-depth metric as WBOIT fog/weight.

---

## Print / validate

```text
hdr_resolve_status
hdr_resolve_validate
```

Static gate: `tests/scripts/test_hdr_resolve_integrity.sh` (also soft-particle depth include).
