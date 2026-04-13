# FBO Breakage Analysis

**Context**: FBO worked yesterday; now broken. Most recent change: OIT draw path re-enabled (commit 0d7afa9d).

**Historical note (2026)**: Descriptor binding for this path lives in `vk_draw_state.c` (`vk_bind_descriptor_sets`); the monolithic `vk.c` no longer exists.

---

## 1. Most Likely Cause: OIT Descriptor Binding Bug

**Location**: `vk_draw_state.c` — `vk_bind_descriptor_sets()`, OIT accum branch

**Bug**: When `backEnd.oitAccumPass` is true, we bind `descriptor_set.current[0]` to pipeline set 0. But in the main pipeline layout:
- **Set 0** = uniform buffer (fog/dlight) → `descriptor_set.current[0]`
- **Set 1** = diffuse texture (tex0) → `descriptor_set.current[1]` = `descriptor_set.current[VK_DESC_TEXTURE0]`

The OIT accum pipeline expects **set 0 = sampler (tex0)**. We are binding the **uniform buffer** descriptor instead of the **texture** descriptor. The OIT accum fragment shader samples `tex0` at set 0 binding 0 — it would be sampling from uniform buffer data as if it were a texture, producing garbage or solid colors.

**Fix**: Bind `descriptor_set.current[VK_DESC_TEXTURE0]` (index 1) for OIT accum, not `current[0]`:

```c
if ( vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0] != VK_NULL_HANDLE ) {
    qvkCmdBindDescriptorSets( ..., vk.cmd->descriptor_set.current + VK_DESC_TEXTURE0, ... );
}
```

---

## 2. When This Bug Triggers

- **r_oit 1** + **r_fbo 1**: OIT path is used. Transparent surfaces are drawn in the OIT accum pass with the wrong descriptor → wrong/solid colors.
- **r_oit 0**: OIT path is skipped. Normal flow; this bug does not apply.

**If FBO is broken with r_oit 0**, the cause is elsewhere (see Section 4).

---

## 3. OIT Path Flow (When r_oit 1)

1. Draw opaque surfaces in main pass.
2. `vk_oit_pass`: end main, copy color→fog_scene, OIT accum (transparent), OIT resolve, begin post_bloom.
3. Sun, flares, etc. drawn in post_bloom.
4. Rest of frame: volumetrics, SMAA, gamma, present.

The mid-frame switch from main to post_bloom was previously identified as a risk. With the OIT accum pipeline wired, the flow is intended to work. The descriptor bug would corrupt transparent rendering specifically.

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

## 6. Recommended Fix

1. Fix OIT accum descriptor binding: use `VK_DESC_TEXTURE0` (index 1) instead of index 0.
2. If FBO is still broken with r_oit 0, investigate descriptor chain, layout transitions, and gamma pass source.
