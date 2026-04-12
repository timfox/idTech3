# FBO Breakage Analysis

**Context**: FBO worked yesterday; now broken. Most recent change: OIT draw path re-enabled (commit 0d7afa9d).

---

## 1. OIT descriptor binding (historical bug, fixed)

**Location**: `vk_draw_state.c` — `vk_bind_descriptor_sets()`, `backEnd.oitAccumPass` branch (Vulkan code was split out of the old monolithic `vk.c`).

**What went wrong (March 2025)**: The OIT accum pipeline expects **set 0 = `tex0`**. The main layout uses **set 0 = uniform** and **`VK_DESC_TEXTURE0` for the diffuse map**, so binding `current[0]` in the accum pass fed the **uniform buffer** to a texture sampler → garbage or solid colors on transparent surfaces.

**Current code**: The accum path binds `vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0]` for set 0 and optionally `vk.oit_depth_descriptor` for set 1. If OIT+FBO mis-renders reappear, re-audit this block and the OIT pipeline layout in-tree.

---

## 2. When the old bug showed up

- **r_oit 1** + **r_fbo 1**: OIT accum ran with the wrong descriptor → broken transparent pass.
- **r_oit 0**: OIT skipped; that specific failure mode did not apply.

**If FBO is broken with r_oit 0**, the cause is elsewhere (see Section 4).

---

## 3. OIT Path Flow (When r_oit 1)

1. Draw opaque surfaces in main pass.
2. `vk_oit_pass`: end main, copy color→fog_scene, OIT accum (transparent), OIT resolve, begin post_bloom.
3. Sun, flares, etc. drawn in post_bloom.
4. Rest of frame: volumetrics, SMAA, gamma, present.

The mid-frame switch from main to post_bloom was previously identified as a risk. With the OIT accum pipeline wired, the flow is intended to work. A bad OIT accum bind corrupts **transparent** rendering specifically.

---

## 4. Other Potential Causes (If r_oit 0)

If FBO is broken even with **r_oit 0**, consider:

| Area | Check |
|------|------|
| **vk_bind_descriptor_sets** | The `start == ~0U && !backEnd.oitAccumPass` change: when `oitAccumPass` is false we return early as before. No regression expected. |
| **vk_bind_pipeline** | When `oitAccumPass` is false, we use `vk_gen_pipeline(pipeline)`. No change to normal path. |
| **vk_update_mvp** | When `oitAccumPass` is false, we push to `vk.pipeline_layout`. No change. |
| **RB_DrawSurfs** | When `r_oit` is 0, we take the `else` branch and call `RB_RenderDrawSurfList` for all surfaces. Same as pre-OIT. |
| **Descriptor updates** | `vk_update_post_fog_descriptors`, `post_fog_color_source` — unchanged by OIT. |
| **Config** | `r_oit` may be 1 in config from prior testing. Try `r_oit 0` and `vid_restart`. |

---

## 5. Quick Workaround

**Disable OIT** to restore FBO:

```
r_oit 0
vid_restart
```

---

## 6. If this regresses

1. Confirm `vk_bind_descriptor_sets` still binds **`VK_DESC_TEXTURE0`** for OIT accum set 0 (and depth set 1 when present).
2. If FBO is still broken with **r_oit 0**, investigate descriptor chain, layout transitions, and gamma pass source (see `docs/VULKAN_FBO_AUDIT.md`).
