# Hi-Z Occlusion

**Status:** Authoritative reversed-Z pyramid (`vk_hiz.c`).  
**Related:** [GPU_VISIBILITY.md](GPU_VISIBILITY.md) · `hiz_downsample.comp`

## Ownership

Scene depth → Hi-Z pyramid → GPU scene occlusion / future SSR.

## Data flow

Depth prepass → downsample mips with **min()** (farthest under reversed-Z) → host coarse sample (≤64², 1-frame lag).

## Buffer formats

`R32_SFLOAT` mip chain. Odd extents supported via extent push constants.

## Lifecycle

`vk_hiz_on_resize` / camera cut invalidates confidence. Missing pyramid → keep visible.

## Fallback behavior

Large projected AABB, grace frames, camera cut → never one-frame pop.

## Debug commands

`hiz_status`, `r_hiZ`, `r_hizDebug` 1–6, `r_hizDebugMip`.

## Performance cost

~0.2–1 ms @ 1080p for full mip chain.

## Known limitations

Host occlusion sample lags one frame; full GPU Hi-Z test in compute is next.

## Next milestone hooks

Bind pyramid views into GPU occlusion compute.
