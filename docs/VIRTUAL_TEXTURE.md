# Virtual texture scaffold

Chocolate **MegaTexture-class lite**: physical page atlas + CPU page table. Not sparse `VkImage` residency and not a full BSP UV rewrite.

## Enable

```
set r_vt 1
vid_restart
vt_status
vt_load textures/demo/dirt.png   // or any path; missing files get a procedural page
vt_flush
```

## Cvars / commands

| | |
|--|--|
| `r_vt` | Latched master (default 0) |
| `vt_status` | Atlas size, slot use, hit/miss |
| `vt_load <path>` | Upload one page into the atlas |
| `vt_flush` | Clear page table |

Atlas: 8×8 pages of 128×128 RGBA (`*vt_atlas`). Uploads use `vk_upload_image_data`.

## Relation to NDGI

Neural Dynamic GI’s `r_ndgi_vt` is **lightmap dirty-page decode**, not this albedo page cache. See [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md).

## Limits (v1)

- No GPU feedback / indirection UVs in world shaders yet
- Demo consumer is console `vt_load` + atlas image for tools/debug
