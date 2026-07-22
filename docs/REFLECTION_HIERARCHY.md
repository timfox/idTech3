# Reflection Hierarchy

**Status:** Foundation Consolidation — ordered reflection source selection.  
**Source:** `vk_reflection_hierarchy.c` · **Legacy alias:** `r_shrDebug` in `vk_selective_reflection.c`

Priority chain: **planar → SSR → ray query → probe → sky**.

---

## Ownership

| Source | Module | When selected |
|--------|--------|---------------|
| Planar | Water / mirror surfaces | Flat reflective planes |
| SSR | Screen-space ray march | Roughness / distance bounds |
| Ray | RTX / ray query (opt-in) | `USE_VULKAN_RTX`, hybrid |
| Probe | Reflection probes / SH | Interior / fallback |
| Sky | Cubemap / atmosphere | Miss / low weight |

`vk_reflection_hierarchy_note()` records last selection for debug.

---

## Data flow

```text
Shading requests reflection radiance
  → try planar (if surface flagged)
  → else SSR trace (G-buffer depth/normal)
  → else ray query (if RTX on)
  → else blend reflection probes
  → else sky cubemap
Composite weight stored in vkReflectionResult_t
r_reflectionDebug 1–6 → owner / mask / composite views
```

`r_reflectionDebug` consolidates hierarchy false-color; maps to `r_shrDebug` composite codes where selective reflection active.

---

## Buffer formats

- SSR: history / hit mask textures (RGBA16 or R8) — path dependent.
- Probes: cubemap or 2D atlas — standard sampler formats.
- Debug: false-color written to SceneHDR overlay (cheat).

No dedicated hierarchy buffer in Foundation Consolidation — result tracked CPU-side in `vkReflectionResult_t`.

---

## Lifecycle

1. Register: `vk_reflection_hierarchy_register()`.
2. Per pixel / surface: selective reflection evaluates chain.
3. Note: `vk_reflection_hierarchy_note(source, weight, note)`.
4. Status: `reflection_hierarchy_status` dumps last / previous source.

---

## Fallback behavior

- SSR miss → probe → sky (never black unless all fail).
- RTX off → ray stage skipped automatically.
- `r_shrDebug` / `r_reflectionDebug 0` — production composite only.
- Hybrid1 / selective reflection may override owner — hierarchy documents intent.

---

## Debug commands

| Cvar / command | Role |
|----------------|------|
| `r_reflectionDebug` | 1 owner, 2 SSR mask, 3 probe weight, 4 RT mask, 5 planar, 6 composite source |
| `reflection_hierarchy_status` | Last source, weight, SHR owner |
| `r_shrDebug` | Legacy selective reflection debug (0–25) |

---

## Performance cost

| Source | Relative cost |
|--------|---------------|
| Planar | Low (extra draw) |
| SSR | Medium (screen trace) |
| Ray | High (RTX) |
| Probe | Low (sample) |
| Sky | Trivial |

Debug overlays: optional ~0.1 ms false-color pass.

---

## Known limitations

- Hierarchy module tracks **last** decision — not per-pixel history buffer.
- `r_reflectionDebug` alias to `r_shrDebug` incomplete for all codes 1–6.
- Water / glass may bypass SSR for specialized refraction paths.

---

## Next milestone hooks

- Per-pixel source id buffer for reference lab reflections scene.
- Unify `r_reflectionDebug` with hybrid1 composite codes.
- CI capture: reflection source distribution on `VK_REFLAB_SCENE_REFLECTIONS`.

Regression: `tests/scripts/test_reflection_fallback.sh`
